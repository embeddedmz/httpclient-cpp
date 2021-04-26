/*
 * @file HTTPClient.h
 * @brief libcurl wrapper for HTTP requests
 *
 * @author Mohamed Amine Mzoughi <mohamed-amine.mzoughi@laposte.net>
 * @date 2017-01-04
 */

#ifndef INCLUDE_HTTPCLIENT_H_
#define INCLUDE_HTTPCLIENT_H_

#define CLIENT_USERAGENT "httpclientcpp-agent/1.0"

#include <algorithm>
#include <atomic>
#include <cstddef>         // std::size_t
#include <cstdio>          // snprintf
#include <cstdlib>
#include <cstring>         // strerror, strlen, memcpy, strcpy
#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>    // std::unique_ptr
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <stdarg.h>  // va_start, etc.
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

#include "CurlHandle.h"

class CHTTPClient
{
public:
   // Public definitions
   typedef std::function<int(void*, double, double, double, double)> ProgressFnCallback;
   typedef std::function<void(const std::string&)>                   LogFnCallback;
   typedef std::unordered_map<std::string, std::string>              HeadersMap;
   typedef std::vector<char> ByteBuffer;

   /* This struct represents the form information to send on POST Form requests */
   struct PostFormInfo
   {
      PostFormInfo();
      ~PostFormInfo();
      
      /* Fill in the file upload field */
      void AddFormFile(const std::string& fieldName, const std::string& fieldValue);
      
      /* Fill in the filename or the submit field */
      void AddFormContent(const std::string& fieldName, const std::string& fieldValue);

      struct curl_httppost* m_pFormPost;
      struct curl_httppost* m_pLastFormptr;
   };

   // Progress Function Data Object - parameter void* of ProgressFnCallback references it
   struct ProgressFnStruct
   {
      ProgressFnStruct() : dLastRunTime(0), pCurl(nullptr), pOwner(nullptr) {}
      double dLastRunTime;
      CURL*  pCurl;
      /* owner of the CHTTPClient object. can be used in the body of the progress
      * function to send signals to the owner (e.g. to update a GUI's progress bar)
      */
      void*  pOwner;
   };

   // HTTP response data
   struct HttpResponse
   {
      HttpResponse() : iCode(0) {}
      int iCode; // HTTP response code
      HeadersMap mapHeaders; // HTTP response headers fields
      std::string strBody; // HTTP response body
   };

   enum SettingsFlag
   {
      NO_FLAGS = 0x00,
      ENABLE_LOG = 0x01,
      VERIFY_PEER = 0x02,
      VERIFY_HOST = 0x04,
      ALL_FLAGS = 0xFF
   };

   /* Please provide your logger thread-safe routine, otherwise, you can turn off
   * error log messages printing by not using the flag ALL_FLAGS or ENABLE_LOG */
   explicit CHTTPClient(LogFnCallback oLogger);
   virtual ~CHTTPClient();

   // copy constructor and assignment operator are disabled
   CHTTPClient(const CHTTPClient& Copy) = delete;
   CHTTPClient& operator=(const CHTTPClient& Copy) = delete;

   // Setters - Getters (for unit tests)
   /*inline*/ void SetProgressFnCallback(void* pOwner, const ProgressFnCallback& fnCallback);
   /*inline*/ void SetProxy(const std::string& strProxy);
   inline void SetTimeout(const int& iTimeout) { m_iCurlTimeout = iTimeout; }
   inline void SetNoSignal(const bool& bNoSignal) { m_bNoSignal = bNoSignal; }
   inline void SetHTTPS(const bool& bEnableHTTPS) { m_bHTTPS = bEnableHTTPS; }
   inline auto GetProgressFnCallback() const
   {
      return m_fnProgressCallback.target<int(*)(void*, double, double, double, double)>();
   }
   inline void* GetProgressFnCallbackOwner() const { return m_ProgressStruct.pOwner; }
   inline const std::string& GetProxy() const { return m_strProxy; }
   inline const int GetTimeout() const { return m_iCurlTimeout; }
   inline const bool GetNoSignal() const { return m_bNoSignal; }
   inline const std::string& GetURL()      const { return m_strURL; }
   inline const unsigned char GetSettingsFlags() const { return m_eSettingsFlags; }
   inline const bool GetHTTPS() const { return m_bHTTPS; }

   // Session
   const bool InitSession(const bool& bHTTPS = false,
                          const SettingsFlag& SettingsFlags = ALL_FLAGS);
   virtual const bool CleanupSession();
   const CURL* GetCurlPointer() const { return m_pCurlSession; }

   // HTTP requests
   const bool GetText(const std::string& strURL,
                      std::string& strText,
                      long& lHTTPStatusCode);

   const bool DownloadFile(const std::string& strLocalFile,
                           const std::string& strURL,
                           long& lHTTPStatusCode);

   const bool UploadForm(const std::string& strURL,
                         const PostFormInfo& data,
                         long& lHTTPStatusCode);

   inline void AddHeader(const std::string& strHeader)
   {
      m_pHeaderlist = curl_slist_append(m_pHeaderlist, strHeader.c_str());
   }

   // REST requests
   const bool Head(const std::string& strUrl, const HeadersMap& Headers, HttpResponse& Response);
   const bool Get(const std::string& strUrl, const HeadersMap& Headers, HttpResponse& Response);
   const bool Del(const std::string& strUrl, const HeadersMap& Headers, HttpResponse& Response);
   const bool Post(const std::string& strUrl, const HeadersMap& Headers,
             const std::string& strPostData, HttpResponse& Response);
   const bool Put(const std::string& strUrl, const HeadersMap& Headers,
            const std::string& strPutData, HttpResponse& Response);
   const bool Put(const std::string& strUrl, const HeadersMap& Headers,
            const ByteBuffer& Data, HttpResponse& Response);
   
   // SSL certs
   static const std::string& GetCertificateFile() { return s_strCertificationAuthorityFile; }
   static void SetCertificateFile(const std::string& strPath) { s_strCertificationAuthorityFile = strPath; }

   void SetSSLCertFile(const std::string& strPath) { m_strSSLCertFile = strPath; }
   const std::string& GetSSLCertFile() const { return m_strSSLCertFile; }

   void SetSSLKeyFile(const std::string& strPath) { m_strSSLKeyFile = strPath; }
   const std::string& GetSSLKeyFile() const { return m_strSSLKeyFile; }

   void SetSSLKeyPassword(const std::string& strPwd) { m_strSSLKeyPwd = strPwd; }
   const std::string& GetSSLKeyPwd() const { return m_strSSLKeyPwd; }

#ifdef DEBUG_CURL
   static void SetCurlTraceLogDirectory(const std::string& strPath);
#endif

protected:
   // payload to upload on POST requests.
   struct UploadObject
   {
      UploadObject() : pszData(nullptr), usLength(0) {}
      const char* pszData; // data to upload
      size_t usLength; // length of the data to upload
   };

   /* common operations are performed here */
   inline const CURLcode Perform();
   inline void CheckURL(const std::string& strURL);
   inline const bool InitRestRequest(const std::string& strUrl, const HeadersMap& Headers,
                               HttpResponse& Response);
   inline const bool PostRestRequest(const CURLcode ePerformCode, HttpResponse& Response);

   // Curl callbacks
   static size_t WriteInStringCallback(void* ptr, size_t size, size_t nmemb, void* data);
   static size_t WriteToFileCallback(void* ptr, size_t size, size_t nmemb, void* data);
   static size_t ReadFromFileCallback(void* ptr, size_t size, size_t nmemb, void* stream);
   static size_t ThrowAwayCallback(void* ptr, size_t size, size_t nmemb, void* data);
   static size_t RestWriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
   static size_t RestHeaderCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
   static size_t RestReadCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
   
   // String Helpers
   static std::string StringFormat(const std::string strFormat, ...);
   static inline void TrimSpaces(std::string& str);

   // Curl Debug informations
#ifdef DEBUG_CURL
   static int DebugCallback(CURL* curl, curl_infotype curl_info_type, char* strace, size_t nSize, void* pFile);
   inline void StartCurlDebug() const;
   inline void EndCurlDebug() const;
#endif

   std::string          m_strURL;
   std::string          m_strProxy;

   bool                 m_bNoSignal;
   bool                 m_bHTTPS;
   SettingsFlag         m_eSettingsFlags;

   struct curl_slist*    m_pHeaderlist;

   // SSL
   static std::string   s_strCertificationAuthorityFile;
   std::string          m_strSSLCertFile;
   std::string          m_strSSLKeyFile;
   std::string          m_strSSLKeyPwd;

   CURL*         m_pCurlSession;
   int           m_iCurlTimeout;

   // Progress function
   ProgressFnCallback    m_fnProgressCallback;
   ProgressFnStruct      m_ProgressStruct;
   bool                  m_bProgressCallbackSet;

   // Log printer callback
   LogFnCallback         m_oLog;

private:
#ifdef DEBUG_CURL
   static std::string s_strCurlTraceLogDirectory;
   mutable std::ofstream      m_ofFileCurlTrace;
#endif

   CurlHandle& m_curlHandle;
};

inline CHTTPClient::SettingsFlag operator|(CHTTPClient::SettingsFlag a, CHTTPClient::SettingsFlag b) {
    return static_cast<CHTTPClient::SettingsFlag>(static_cast<int>(a) | static_cast<int>(b));
}

// Logs messages
#define LOG_ERROR_EMPTY_HOST_MSG                "[HTTPClient][Error] Empty hostname."
#define LOG_WARNING_OBJECT_NOT_CLEANED          "[HTTPClient][Warning] Object was freed before calling " \
                                                "CHTTPClient::CleanupSession(). The API session was cleaned though."
#define LOG_ERROR_CURL_ALREADY_INIT_MSG         "[HTTPClient][Error] Curl session is already initialized ! " \
                                                "Use CleanupSession() to clean the present one."
#define LOG_ERROR_CURL_NOT_INIT_MSG             "[HTTPClient][Error] Curl session is not initialized ! Use InitSession() before."


#define LOG_ERROR_CURL_REQ_FAILURE_FORMAT       "[HTTPClient][Error] Unable to perform request from '%s' " \
                                                "(Error = %d | %s) (HTTP_Status = %ld)"
#define LOG_ERROR_CURL_REST_FAILURE_FORMAT      "[HTTPClient][Error] Unable to perform a REST request from '%s' " \
                                                "(Error = %d | %s)"
#define LOG_ERROR_CURL_DOWNLOAD_FAILURE_FORMAT  "[HTTPClient][Error] Unable to perform a request - '%s' from '%s' " \
                                                "(Error = %d | %s) (HTTP_Status = %ld)"
#define LOG_ERROR_DOWNLOAD_FILE_FORMAT          "[HTTPClient][Error] Unable to open local file %s"

#endif
