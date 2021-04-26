/**
* @file HTTPClient.cpp
* @brief implementation of the HTTP client class
* @author Mohamed Amine Mzoughi <mohamed-amine.mzoughi@laposte.net>
*/

#include "HTTPClient.h"

// Static members initialization
std::string    CHTTPClient::s_strCertificationAuthorityFile;

#ifdef DEBUG_CURL
std::string CHTTPClient::s_strCurlTraceLogDirectory;
#endif

/**
 * @brief constructor of the HTTP client object
 *
 * @param Logger - a callabck to a logger function void(const std::string&)
 *
 */
CHTTPClient::CHTTPClient(LogFnCallback Logger) :
   m_oLog(Logger),
   m_iCurlTimeout(0),
   m_bHTTPS(false),
   m_bNoSignal(false),
   m_bProgressCallbackSet(false),
   m_eSettingsFlags(ALL_FLAGS),
   m_pCurlSession(nullptr),
   m_pHeaderlist(nullptr),
   m_curlHandle(CurlHandle::instance())
{

}

/**
 * @brief destructor of the HTTP client object
 *
 */
CHTTPClient::~CHTTPClient()
{
   if (m_pCurlSession != nullptr)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_WARNING_OBJECT_NOT_CLEANED);

      CleanupSession();
   }
}

/**
 * @brief Starts a new HTTP session, initializes the cURL API session
 *
 * If a new session was already started, the method has no effect.
 *
 * @param [in] bHTTPS Enable/Disable HTTPS (disabled by default)
 * @param [in] eSettingsFlags optional use | operator to choose multiple options
 *
 * @retval true   Successfully initialized the session.
 * @retval false  The session is already initialized
 * Use CleanupSession() before initializing a new one or the Curl API is not initialized.
 *
 * Example Usage:
 * @code
 *    m_pHTTPClient->InitSession();
 * @endcode
 */
const bool CHTTPClient::InitSession(const bool& bHTTPS /* = false */,
                                    const SettingsFlag& eSettingsFlags /* = ALL_FLAGS */)
{
   if (m_pCurlSession)
   {
      if (eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_CURL_ALREADY_INIT_MSG);

      return false;
   }
   m_pCurlSession = curl_easy_init();

   m_bHTTPS = bHTTPS;
   m_eSettingsFlags = eSettingsFlags;

   return (m_pCurlSession != nullptr);
}

/**
 * @brief Cleans the current HTTP session
 *
 * If a session was not already started, the method has no effect
 *
 * @retval true   Successfully cleaned the current session.
 * @retval false  The session is not already initialized.
 *
 * Example Usage:
 * @code
 *    m_pHTTPClient->CleanupSession();
 * @endcode
 */
const bool CHTTPClient::CleanupSession()
{
   if (!m_pCurlSession)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }

#ifdef DEBUG_CURL
   if (m_ofFileCurlTrace.is_open())
   {
      m_ofFileCurlTrace.close();
   }
#endif

   curl_easy_cleanup(m_pCurlSession);
   m_pCurlSession = nullptr;

   if (m_pHeaderlist)
   {
      curl_slist_free_all(m_pHeaderlist);
      m_pHeaderlist = nullptr;
   }

   return true;
}

/**
 * @brief sets the progress function callback and the owner of the client
 *
 * @param [in] pOwner pointer to the object owning the client, nullptr otherwise
 * @param [in] fnCallback callback to progress function
 *
 */
/*inline*/ void CHTTPClient::SetProgressFnCallback(void* pOwner, const ProgressFnCallback& fnCallback)
{
   m_ProgressStruct.pOwner = pOwner;
   m_fnProgressCallback = fnCallback;
   m_ProgressStruct.pCurl = m_pCurlSession;
   m_ProgressStruct.dLastRunTime = 0;
   m_bProgressCallbackSet = true;
}

/**
 * @brief sets the HTTP Proxy address to tunnel the operation through it
 *
 * @param [in] strProxy URI of the HTTP Proxy
 *
 */
/*inline*/ void CHTTPClient::SetProxy(const std::string& strProxy)
{
   if (strProxy.empty())
      return;

   std::string strUri = strProxy;
   std::transform(strUri.begin(), strUri.end(), strUri.begin(), ::toupper);

   if (strUri.compare(0, 4, "HTTP") != 0)
      m_strProxy = "http://" + strProxy;
   else
      m_strProxy = strProxy;
};

/**
 * @brief checks a URI
 * adds the proper protocol scheme (HTTP:// or HTTPS://)
 * if the URI has no protocol scheme, the added protocol scheme
 * will depend on m_bHTTPS that can be set when initializing a session
 * or with the SetHTTPS(bool)
 *
 * @param [in] strURL user URI
 */
inline void CHTTPClient::CheckURL(const std::string& strURL)
{
   std::string strTmp = strURL;

   std::transform(strTmp.begin(), strTmp.end(), strTmp.begin(), ::toupper);

   if (strTmp.compare(0, 7, "HTTP://") == 0)
      m_bHTTPS = false;
   else if (strTmp.compare(0, 8, "HTTPS://") == 0)
      m_bHTTPS = true;
   else
   {
      m_strURL = ((m_bHTTPS) ? "https://" : "http://") + strURL;
      return;
   }
   m_strURL = strURL;
}

/**
* @brief performs the chosen HTTP request
* sets up the common settings (Timeout, proxy,...)
*
*
* @retval true   Successfully performed the request.
* @retval false  An error occured while CURL was performing the request.
*/
const CURLcode CHTTPClient::Perform()
{
   if (!m_pCurlSession)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return CURLE_FAILED_INIT;
   }

   CURLcode res = CURLE_OK;

   curl_easy_setopt(m_pCurlSession, CURLOPT_URL, m_strURL.c_str());

   if (m_pHeaderlist != nullptr)
      curl_easy_setopt(m_pCurlSession, CURLOPT_HTTPHEADER, m_pHeaderlist);

   curl_easy_setopt(m_pCurlSession, CURLOPT_USERAGENT, CLIENT_USERAGENT);
   curl_easy_setopt(m_pCurlSession, CURLOPT_AUTOREFERER, 1L);
   curl_easy_setopt(m_pCurlSession, CURLOPT_FOLLOWLOCATION, 1L);

   if (m_iCurlTimeout > 0)
   {
      curl_easy_setopt(m_pCurlSession, CURLOPT_TIMEOUT, m_iCurlTimeout);
      // don't want to get a sig alarm on timeout
      curl_easy_setopt(m_pCurlSession, CURLOPT_NOSIGNAL, 1);
   }

   if (!m_strProxy.empty())
   {
      curl_easy_setopt(m_pCurlSession, CURLOPT_PROXY, m_strProxy.c_str());
      curl_easy_setopt(m_pCurlSession, CURLOPT_HTTPPROXYTUNNEL, 1L);      
   }

   if (m_bNoSignal)
   {
      curl_easy_setopt(m_pCurlSession, CURLOPT_NOSIGNAL, 1L);
   }

   if (m_bProgressCallbackSet)
   {
      curl_easy_setopt(m_pCurlSession, CURLOPT_PROGRESSFUNCTION, *GetProgressFnCallback());
      curl_easy_setopt(m_pCurlSession, CURLOPT_PROGRESSDATA, &m_ProgressStruct);
      curl_easy_setopt(m_pCurlSession, CURLOPT_NOPROGRESS, 0L);
   }

   if (m_bHTTPS)
   {
       // SSL (TLS)
       curl_easy_setopt(m_pCurlSession, CURLOPT_USE_SSL, CURLUSESSL_ALL);
       curl_easy_setopt(m_pCurlSession, CURLOPT_SSL_VERIFYPEER, (m_eSettingsFlags & VERIFY_PEER) ? 1L : 0L);
       curl_easy_setopt(m_pCurlSession, CURLOPT_SSL_VERIFYPEER, (m_eSettingsFlags & CURLOPT_SSL_VERIFYHOST) ? 2L : 0L);
   }

   if (m_bHTTPS && !s_strCertificationAuthorityFile.empty())
      curl_easy_setopt(m_pCurlSession, CURLOPT_CAINFO, s_strCertificationAuthorityFile.c_str());

   if (m_bHTTPS && !m_strSSLCertFile.empty())
      curl_easy_setopt(m_pCurlSession, CURLOPT_SSLCERT, m_strSSLCertFile.c_str());

   if (m_bHTTPS && !m_strSSLKeyFile.empty())
      curl_easy_setopt(m_pCurlSession, CURLOPT_SSLKEY, m_strSSLKeyFile.c_str());

   if (m_bHTTPS && !m_strSSLKeyPwd.empty())
      curl_easy_setopt(m_pCurlSession, CURLOPT_KEYPASSWD, m_strSSLKeyPwd.c_str());

#ifdef DEBUG_CURL
   StartCurlDebug();
#endif

   // Perform the requested operation
   res = curl_easy_perform(m_pCurlSession);

#ifdef DEBUG_CURL
   EndCurlDebug();
#endif

   if (m_pHeaderlist)
   {
      curl_slist_free_all(m_pHeaderlist);
      m_pHeaderlist = nullptr;
   }

   return res;
}

/**
 * @brief requests the content of a URI
 *
 * @param [in] strURL URI of the remote location (with the file name).
 * @param [out] strOutput reference to an output string.
 * @param [out] lHTTPStatusCode HTTP Status code of the response.
 *
 * @retval true   Successfully requested the URI.
 * @retval false  Encountered a problem.
 *
 * Example Usage:
 * @code
 *    std::string strWebPage;
 *    long lHTTPStatusCode = 0;
 *    m_pHTTOClient->GetText("https://www.google.com", strWebPage, lHTTPStatusCode);
 * @endcode
 */
const bool CHTTPClient::GetText(const std::string& strURL,
                                std::string& strOutput,
                                long& lHTTPStatusCode)
{
   if (strURL.empty())
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_EMPTY_HOST_MSG);

      return false;
   }
   if (!m_pCurlSession)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   CheckURL(strURL);

   curl_easy_setopt(m_pCurlSession, CURLOPT_HTTPGET, 1L);
   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEFUNCTION, WriteInStringCallback);
   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEDATA, &strOutput);

   CURLcode res = Perform();

   curl_easy_getinfo(m_pCurlSession, CURLINFO_RESPONSE_CODE, &lHTTPStatusCode);

   // Check for errors
   if (res != CURLE_OK)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat(LOG_ERROR_CURL_REQ_FAILURE_FORMAT, m_strURL.c_str(), res,
                             curl_easy_strerror(res), lHTTPStatusCode));

      return false;
   }

   return true;
}

/**
 * @brief Downloads a remote file to a local file.
 *
 * @param [in] strLocalFile Complete path of the local file to download.
 * @param [in] strURL URI of the remote location (with the file name).
 * @param [out] lHTTPStatusCode HTTP Status code of the response.
 *
 * @retval true   Successfully downloaded the file.
 * @retval false  The file couldn't be downloaded. Check the log messages for more information.
 */
const bool CHTTPClient::DownloadFile(const std::string& strLocalFile,
                                     const std::string& strURL,
                                     long& lHTTPStatusCode)
{
   if (strURL.empty() || strLocalFile.empty())
      return false;

   if (!m_pCurlSession)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   CheckURL(strURL);

   std::ofstream ofsOutput;
   ofsOutput.open(strLocalFile, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);

   if (ofsOutput)
   {
      curl_easy_setopt(m_pCurlSession, CURLOPT_HTTPGET, 1L);
      curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEFUNCTION, WriteToFileCallback);
      curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEDATA, &ofsOutput);

      CURLcode res = Perform();

      ofsOutput.close();
      curl_easy_getinfo(m_pCurlSession, CURLINFO_RESPONSE_CODE, &lHTTPStatusCode);

      //double dUploadLength = 0;
      //curl_easy_getinfo(m_pCurlSession, CURLINFO_CONTENT_LENGTH_UPLOAD, &dUploadLength); // number of bytes uploaded
      
      /* Delete downloaded file if status code != 200 as server's response body may
      contain error 404 */
      if (lHTTPStatusCode != 200)
         remove(strLocalFile.c_str());

      if (res != CURLE_OK)
      {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog(StringFormat(LOG_ERROR_CURL_DOWNLOAD_FAILURE_FORMAT, strLocalFile.c_str(),
               strURL.c_str(), res, curl_easy_strerror(res), lHTTPStatusCode));

         return false;
      }
   }
   else if (m_eSettingsFlags & ENABLE_LOG)
   {
      m_oLog(StringFormat(LOG_ERROR_DOWNLOAD_FILE_FORMAT, strLocalFile.c_str()));

      return false;
   }

   return true;
}

/**
 * @brief uploads a POST form
 *
 *
 * @param [in] strURL URL to which the form will be posted.
 * @param [in] data post form information
 * @param [out] lHTTPStatusCode HTTP Status code of the response.
 *
 * @retval true   Successfully posted the header.
 * @retval false  The header couldn't be posted.
 */
const bool CHTTPClient::UploadForm(const std::string& strURL,
                                   const PostFormInfo& data,
                                   long& lHTTPStatusCode)
{
   if (strURL.empty())
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_EMPTY_HOST_MSG);

      return false;
   }
   if (!m_pCurlSession)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   CheckURL(strURL);

   /** Now specify we want to POST data */
   curl_easy_setopt(m_pCurlSession, CURLOPT_POST, 1L);

   /* stating that Expect: 100-continue is not wanted */
   AddHeader("Expect:");
   
   /** set post form */
   if (data.m_pFormPost != nullptr)
      curl_easy_setopt(m_pCurlSession, CURLOPT_HTTPPOST, data.m_pFormPost);

   /* to avoid printing response's body to stdout.
    * CURLOPT_WRITEDATA : by default, this is a FILE * to stdout. */
   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEFUNCTION, ThrowAwayCallback);

   CURLcode res = Perform();

   curl_easy_getinfo(m_pCurlSession, CURLINFO_RESPONSE_CODE, &lHTTPStatusCode);

   // Check for errors
   if (res != CURLE_OK)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat(LOG_ERROR_CURL_REQ_FAILURE_FORMAT, m_strURL.c_str(), res,
            curl_easy_strerror(res), lHTTPStatusCode));

      return false;
   }

   return true;
}

/**
 * @brief PostFormInfo constructor
 */
CHTTPClient::PostFormInfo::PostFormInfo() :
   m_pFormPost(nullptr), m_pLastFormptr(nullptr)
{
}

/**
 * @brief PostFormInfo destructor
 */
CHTTPClient::PostFormInfo::~PostFormInfo()
{
   // cleanup the formpost chain
   if (m_pFormPost)
   {
      curl_formfree(m_pFormPost);
      m_pFormPost = nullptr;
      m_pLastFormptr = nullptr;
   }
}

/**
 * @brief set the name and the value of the HTML "file" form's input
 *
 * @param fieldName name of the "file" input
 * @param fieldValue path to the file to upload
 */
void CHTTPClient::PostFormInfo::AddFormFile(const std::string& strFieldName,
                                            const std::string& strFieldValue)
{
   curl_formadd(&m_pFormPost, &m_pLastFormptr,
      CURLFORM_COPYNAME, strFieldName.c_str(),
      CURLFORM_FILE, strFieldValue.c_str(),
      CURLFORM_END);
}

/**
 * @brief set the name and the value of an HTML form's input
 * (other than "file" like "text", "hidden" or "submit")
 *
 * @param fieldName name of the input element
 * @param fieldValue value to be assigned to the input element
 */
void CHTTPClient::PostFormInfo::AddFormContent(const std::string& strFieldName,
                                               const std::string& strFieldValue)
{
   curl_formadd(&m_pFormPost, &m_pLastFormptr,
      CURLFORM_COPYNAME, strFieldName.c_str(),
      CURLFORM_COPYCONTENTS, strFieldValue.c_str(),
      CURLFORM_END);
}

// REST REQUESTS

/**
* @brief initializes a REST request
* some common operations to REST requests are performed here,
* the others are performed in Perform method
*
* @param [in] Headers headers to send
* @param [out] Response response data
*/
inline const bool CHTTPClient::InitRestRequest(const std::string& strUrl,
                                         const CHTTPClient::HeadersMap& Headers,
                                         CHTTPClient::HttpResponse& Response)
{
   if (strUrl.empty())
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_EMPTY_HOST_MSG);

      return false;
   }
   if (!m_pCurlSession)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   CheckURL(strUrl);

   // set the received body's callback function
   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEFUNCTION, &CHTTPClient::RestWriteCallback);

   // set data object to pass to callback function above
   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEDATA, &Response);

   // set the response's headers processing callback function
   curl_easy_setopt(m_pCurlSession, CURLOPT_HEADERFUNCTION, &CHTTPClient::RestHeaderCallback);

   // callback object for server's responses headers
   curl_easy_setopt(m_pCurlSession, CURLOPT_HEADERDATA, &Response);

   std::string strHeader;
   for (HeadersMap::const_iterator it = Headers.cbegin();
      it != Headers.cend();
      ++it)
   {
      strHeader = it->first + ": " + it->second; // build header string
      AddHeader(strHeader);
   }

   return true;
}

/**
* @brief post REST request operations are performed here
*
* @param [in] ePerformCode curl easy perform returned code
* @param [out] Response response data
*/
inline const bool CHTTPClient::PostRestRequest(const CURLcode ePerformCode,
                                               CHTTPClient::HttpResponse& Response)
{
   // Check for errors
   if (ePerformCode != CURLE_OK)
   {
      Response.strBody.clear();
      Response.iCode = -1;

      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat(LOG_ERROR_CURL_REST_FAILURE_FORMAT, m_strURL.c_str(), ePerformCode,
            curl_easy_strerror(ePerformCode)));

      return false;
   }
   long lHttpCode = 0;
   curl_easy_getinfo(m_pCurlSession, CURLINFO_RESPONSE_CODE, &lHttpCode);
   Response.iCode = static_cast<int>(lHttpCode);

   return true;
}

/**
* @brief performs a HEAD request
*
* @param [in] strUrl url to request
* @param [in] Headers headers to send
* @param [out] Response response data
*
* @retval true   Successfully requested the URI.
* @retval false  Encountered a problem.
*/
const bool CHTTPClient::Head(const std::string& strUrl,
   const CHTTPClient::HeadersMap& Headers,
   CHTTPClient::HttpResponse& Response)
{
   if (InitRestRequest(strUrl, Headers, Response))
   {
      /** set HTTP HEAD METHOD */
      curl_easy_setopt(m_pCurlSession, CURLOPT_CUSTOMREQUEST, "HEAD");
      curl_easy_setopt(m_pCurlSession, CURLOPT_NOBODY, 1L);

      CURLcode res = Perform();

      return PostRestRequest(res, Response);
   }
   else
      return false;
}

/**
* @brief performs a GET request
*
* @param [in] strUrl url to request
* @param [in] Headers headers to send
* @param [out] Response response data
*
* @retval true   Successfully requested the URI.
* @retval false  Encountered a problem.
*/
const bool CHTTPClient::Get(const std::string& strUrl,
   const CHTTPClient::HeadersMap& Headers,
   CHTTPClient::HttpResponse& Response)
{
   if (InitRestRequest(strUrl, Headers, Response))
   {
      // specify a GET request
      curl_easy_setopt(m_pCurlSession, CURLOPT_HTTPGET, 1L);

      CURLcode res = Perform();

      return PostRestRequest(res, Response);
   }
   else
      return false;
}

/**
* @brief performs a DELETE request
*
* @param [in] strUrl url to request
* @param [in] Headers headers to send
* @param [out] Response response data
*
* @retval true   Successfully requested the URI.
* @retval false  Encountered a problem.
*/
const bool CHTTPClient::Del(const std::string& strUrl,
   const CHTTPClient::HeadersMap& Headers,
   CHTTPClient::HttpResponse& Response)
{
   if (InitRestRequest(strUrl, Headers, Response))
   {
      curl_easy_setopt(m_pCurlSession, CURLOPT_CUSTOMREQUEST, "DELETE");

      CURLcode res = Perform();

      return PostRestRequest(res, Response);
   }
   else
      return false;
}

const bool CHTTPClient::Post(const std::string& strUrl,
   const CHTTPClient::HeadersMap& Headers,
   const std::string& strPostData,
   CHTTPClient::HttpResponse& Response)
{
   if (InitRestRequest(strUrl, Headers, Response))
   {
      // specify a POST request
      curl_easy_setopt(m_pCurlSession, CURLOPT_POST, 1L);

      // set post informations
      curl_easy_setopt(m_pCurlSession, CURLOPT_POSTFIELDS, strPostData.c_str());
      curl_easy_setopt(m_pCurlSession, CURLOPT_POSTFIELDSIZE, strPostData.size());

      CURLcode res = Perform();

      return PostRestRequest(res, Response);
   }
   else
      return false;
}

/**
* @brief performs a PUT request with a string
*
* @param [in] strUrl url to request
* @param [in] Headers headers to send
* @param [out] Response response data
*
* @retval true   Successfully requested the URI.
* @retval false  Encountered a problem.
*/
const bool CHTTPClient::Put(const std::string& strUrl, const CHTTPClient::HeadersMap& Headers,
   const std::string& strPutData, CHTTPClient::HttpResponse& Response)
{
   if (InitRestRequest(strUrl, Headers, Response))
   {
      CHTTPClient::UploadObject Payload;

      Payload.pszData = strPutData.c_str();
      Payload.usLength = strPutData.size();

      // specify a PUT request
      curl_easy_setopt(m_pCurlSession, CURLOPT_PUT, 1L);
      curl_easy_setopt(m_pCurlSession, CURLOPT_UPLOAD, 1L);

      // set read callback function
      curl_easy_setopt(m_pCurlSession, CURLOPT_READFUNCTION, &CHTTPClient::RestReadCallback);
      // set data object to pass to callback function
      curl_easy_setopt(m_pCurlSession, CURLOPT_READDATA, &Payload);

      // set data size
      curl_easy_setopt(m_pCurlSession, CURLOPT_INFILESIZE, static_cast<long>(Payload.usLength));

      CURLcode res = Perform();

      return PostRestRequest(res, Response);
   }
   else
      return false;
}

/**
* @brief performs a PUT request with a byte buffer (vector of char)
*
* @param [in] strUrl url to request
* @param [in] Headers headers to send
* @param [out] Response response data
*
* @retval true   Successfully requested the URI.
* @retval false  Encountered a problem.
*/
const bool CHTTPClient::Put(const std::string& strUrl, const CHTTPClient::HeadersMap& Headers,
   const CHTTPClient::ByteBuffer& Data, CHTTPClient::HttpResponse& Response)
{
   if (InitRestRequest(strUrl, Headers, Response))
   {
      CHTTPClient::UploadObject Payload;

      Payload.pszData = Data.data();
      Payload.usLength = Data.size();

      // specify a PUT request
      curl_easy_setopt(m_pCurlSession, CURLOPT_PUT, 1L);
      curl_easy_setopt(m_pCurlSession, CURLOPT_UPLOAD, 1L);

      // set read callback function
      curl_easy_setopt(m_pCurlSession, CURLOPT_READFUNCTION, &CHTTPClient::RestReadCallback);
      // set data object to pass to callback function
      curl_easy_setopt(m_pCurlSession, CURLOPT_READDATA, &Payload);

      // set data size
      curl_easy_setopt(m_pCurlSession, CURLOPT_INFILESIZE, static_cast<long>(Payload.usLength));

      CURLcode res = Perform();

      return PostRestRequest(res, Response);
   }
   else
      return false;
}

// STRING HELPERS

/**
* @brief returns a formatted string
*
* @param [in] strFormat string with one or many format specifiers
* @param [in] parameters to be placed in the format specifiers of strFormat
*
* @retval string formatted string
*/
std::string CHTTPClient::StringFormat(const std::string strFormat, ...)
{
   int n = (static_cast<int>(strFormat.size())) * 2; // Reserve two times as much as the length of the strFormat
   
   std::unique_ptr<char[]> pFormatted;

   va_list ap;

   while(true)
   {
      pFormatted.reset(new char[n]); // Wrap the plain char array into the unique_ptr
      strcpy(&pFormatted[0], strFormat.c_str());
      
      va_start(ap, strFormat);
      int iFinaln = vsnprintf(&pFormatted[0], n, strFormat.c_str(), ap);
      va_end(ap);
      
      if (iFinaln < 0 || iFinaln >= n)
      {
         n += abs(iFinaln - n + 1);
      }
      else
      {
         break;
      }
   }

   return std::string(pFormatted.get());
}
/*std::string CHTTPClient::StringFormat(const std::string strFormat, ...)
{
   char buffer[1024];
   va_list args;
   va_start(args, strFormat);
   vsnprintf(buffer, 1024, strFormat.c_str(), args);
   va_end (args);
   return std::string(buffer);
}
*/

/**
* @brief removes leading and trailing whitespace from a string
*
* @param [in/out] str string to be trimmed
*/
inline void CHTTPClient::TrimSpaces(std::string& str)
{
   // trim from left
   str.erase(str.begin(),
      std::find_if(str.begin(), str.end(), [](char c) {return !isspace(c); })
   );

   // trim from right
   str.erase(std::find_if(str.rbegin(), str.rend(), [](char c) {return !isspace(c); }).base(),
      str.end()
   );
}

// CURL CALLBACKS

size_t CHTTPClient::ThrowAwayCallback(void* ptr, size_t size, size_t nmemb, void* data)
{
   reinterpret_cast<void*>(ptr);
   reinterpret_cast<void*>(data);

   /* we are not interested in the headers itself,
   so we only return the size we would have saved ... */
   return size * nmemb;
}

/**
* @brief stores the server response in a string
*
* @param ptr pointer of max size (size*nmemb) to read data from it
* @param size size parameter
* @param nmemb memblock parameter
* @param data pointer to user data (string)
*
* @return (size * nmemb)
*/
size_t CHTTPClient::WriteInStringCallback(void* ptr, size_t size, size_t nmemb, void* data)
{
   std::string* strWriteHere = reinterpret_cast<std::string*>(data);
   if (strWriteHere != nullptr)
   {
      strWriteHere->append(reinterpret_cast<char*>(ptr), size * nmemb);
      return size * nmemb;
   }
   return 0;
}

/**
* @brief stores the server response in an already opened file stream
* used by DownloadFile()
*
* @param buff pointer of max size (size*nmemb) to read data from it
* @param size size parameter
* @param nmemb memblock parameter
* @param userdata pointer to user data (file stream)
*
* @return (size * nmemb)
*/
size_t CHTTPClient::WriteToFileCallback(void* buff, size_t size, size_t nmemb, void* data)
{
   if ((size == 0) || (nmemb == 0) || ((size*nmemb) < 1) || (data == nullptr))
      return 0;

   std::ofstream* pFileStream = reinterpret_cast<std::ofstream*>(data);
   if (pFileStream->is_open())
   {
      pFileStream->write(reinterpret_cast<char*>(buff), size * nmemb);
   }

   return size * nmemb;
}

/**
* @brief reads the content of an already opened file stream
* used by UploadFile()
*
* @param ptr pointer of max size (size*nmemb) to write data to it
* @param size size parameter
* @param nmemb memblock parameter
* @param stream pointer to user data (file stream)
*
* @return (size * nmemb)
*/
size_t CHTTPClient::ReadFromFileCallback(void* ptr, size_t size, size_t nmemb, void* stream)
{
   std::ifstream* pFileStream = reinterpret_cast<std::ifstream*>(stream);
   if (pFileStream->is_open())
   {
      pFileStream->read(reinterpret_cast<char*>(ptr), size * nmemb);
      return pFileStream->gcount();
   }
   return 0;
}

// REST CALLBACKS

/**
* @brief write callback function for libcurl
* this callback will be called to store the server's Body reponse
* in a struct response
*
* we can also use an std::vector<char> instead of an std::string but in this case
* there isn't a big difference... maybe resizing the container with a max size can
* enhance performances...
*
* @param data returned data of size (size*nmemb)
* @param size size parameter
* @param nmemb memblock parameter
* @param userdata pointer to user data to save/work with return data
*
* @return (size * nmemb)
*/
size_t CHTTPClient::RestWriteCallback(void* pCurlData, size_t usBlockCount, size_t usBlockSize, void* pUserData)
{
   CHTTPClient::HttpResponse* pServerResponse;
   pServerResponse = reinterpret_cast<CHTTPClient::HttpResponse*>(pUserData);
   pServerResponse->strBody.append(reinterpret_cast<char*>(pCurlData), usBlockCount * usBlockSize);

   return (usBlockCount * usBlockSize);
}

/**
* @brief header callback for libcurl
* callback used to process response's headers (received)
*
* @param data returned (header line)
* @param size of data
* @param nmemb memblock
* @param userdata pointer to user data object to save header data
* @return size * nmemb;
*/
size_t CHTTPClient::RestHeaderCallback(void* pCurlData, size_t usBlockCount, size_t usBlockSize, void* pUserData)
{
   CHTTPClient::HttpResponse* pServerResponse;
   pServerResponse = reinterpret_cast<CHTTPClient::HttpResponse*>(pUserData);

   std::string strHeader(reinterpret_cast<char*>(pCurlData), usBlockCount * usBlockSize);
   size_t usSeperator = strHeader.find_first_of(":");
   if (std::string::npos == usSeperator)
   {
      //roll with non seperated headers or response's line
      TrimSpaces(strHeader);
      if (0 == strHeader.length())
      {
         return (usBlockCount * usBlockSize); //blank line;
      }
      pServerResponse->mapHeaders[strHeader] = "present";
   }
   else
   {
      std::string strKey = strHeader.substr(0, usSeperator);
      TrimSpaces(strKey);
      std::string strValue = strHeader.substr(usSeperator + 1);
      TrimSpaces(strValue);
      pServerResponse->mapHeaders[strKey] = strValue;
   }

   return (usBlockCount * usBlockSize);
}

/**
* @brief read callback function for libcurl
* used to send (or upload) a content to the server
*
* @param pointer of max size (size*nmemb) to write data to (used by cURL to send data)
* @param size size parameter
* @param nmemb memblock parameter
* @param userdata pointer to user data to read data from
*
* @return (size * nmemb)
*/
size_t CHTTPClient::RestReadCallback(void* pCurlData, size_t usBlockCount, size_t usBlockSize, void* pUserData)
{
   // get upload struct
   CHTTPClient::UploadObject* Payload;

   Payload = reinterpret_cast<CHTTPClient::UploadObject*>(pUserData);

   // set correct sizes
   size_t usCurlSize = usBlockCount * usBlockSize;
   size_t usCopySize = (Payload->usLength < usCurlSize) ? Payload->usLength : usCurlSize;

   /** copy data to buffer */
   std::memcpy(pCurlData, Payload->pszData, usCopySize);

   // decrement length and increment data pointer
   Payload->usLength -= usCopySize; // remaining bytes to be sent
   Payload->pszData += usCopySize;  // next byte to the chunk that will be sent

                                    /** return copied size */
   return usCopySize;
}

// CURL DEBUG INFO CALLBACKS

#ifdef DEBUG_CURL
void CHTTPClient::SetCurlTraceLogDirectory(const std::string& strPath)
{
   s_strCurlTraceLogDirectory = strPath;

   if (!s_strCurlTraceLogDirectory.empty()
#ifdef WINDOWS
      && s_strCurlTraceLogDirectory.at(s_strCurlTraceLogDirectory.length() - 1) != '\\')
   {
      s_strCurlTraceLogDirectory += '\\';
   }
#else
      && s_strCurlTraceLogDirectory.at(s_strCurlTraceLogDirectory.length() - 1) != '/')
   {
      s_strCurlTraceLogDirectory += '/';
   }
#endif
}

int CHTTPClient::DebugCallback(CURL* curl, curl_infotype curl_info_type, char* pszTrace, size_t usSize, void* pFile)
{
   std::string strText;
   std::string strTrace(pszTrace, usSize);

   switch (curl_info_type)
   {
   case CURLINFO_TEXT:
      strText = "# Information : ";
      break;
   case CURLINFO_HEADER_OUT:
      strText = "-> Sending header : ";
      break;
   case CURLINFO_DATA_OUT:
      strText = "-> Sending data : ";
      break;
   case CURLINFO_SSL_DATA_OUT:
      strText = "-> Sending SSL data : ";
      break;
   case CURLINFO_HEADER_IN:
      strText = "<- Receiving header : ";
      break;
   case CURLINFO_DATA_IN:
      strText = "<- Receiving unencrypted data : ";
      break;
   case CURLINFO_SSL_DATA_IN:
      strText = "<- Receiving SSL data : ";
      break;
   default:
      break;
   }

   std::ofstream* pofTraceFile = reinterpret_cast<std::ofstream*>(pFile);
   if (pofTraceFile == nullptr)
   {
      std::cout << "[DEBUG] cURL debug log [" << curl_info_type << "]: " << " - " << strTrace;
   }
   else
   {
      (*pofTraceFile) << strText << strTrace;
   }

   return 0;
}

void CHTTPClient::StartCurlDebug() const
{
   if (!m_ofFileCurlTrace.is_open())
   {
      curl_easy_setopt(m_pCurlSession, CURLOPT_VERBOSE, 1L);
      curl_easy_setopt(m_pCurlSession, CURLOPT_DEBUGFUNCTION, DebugCallback);

      std::string strFileCurlTraceFullName(s_strCurlTraceLogDirectory);
      if (!strFileCurlTraceFullName.empty())
      {
         char szDate[32];
         memset(szDate, 0, 32);
         time_t tNow; time(&tNow);
         // new trace file for each hour
         strftime(szDate, 32, "%Y%m%d_%H", localtime(&tNow));
         strFileCurlTraceFullName += "TraceLog_";
         strFileCurlTraceFullName += szDate;
         strFileCurlTraceFullName += ".txt";

         m_ofFileCurlTrace.open(strFileCurlTraceFullName, std::ifstream::app | std::ifstream::binary);

         if (m_ofFileCurlTrace)
            curl_easy_setopt(m_pCurlSession, CURLOPT_DEBUGDATA, &m_ofFileCurlTrace);
      }
   }
}

void CHTTPClient::EndCurlDebug() const
{
   if (m_ofFileCurlTrace && m_ofFileCurlTrace.is_open())
   {
      m_ofFileCurlTrace << "###########################################" << std::endl;
      m_ofFileCurlTrace.close();
   }
}
#endif
