#include "test_utils.h"

// Test configuration constants (to be loaded from an INI file)
bool HTTP_PROXY_TEST_ENABLED;

std::string CURL_LOG_FOLDER;

std::string CERT_AUTH_FILE;
std::string SSL_CERT_FILE;
std::string SSL_KEY_FILE;
std::string SSL_KEY_PWD;

std::string PROXY_SERVER;
std::string PROXY_SERVER_FAKE;

std::mutex g_mtxConsoleMutex;

bool GlobalTestInit(const std::string& strConfFile)
{
   CSimpleIniA ini;
   ini.LoadFile(strConfFile.c_str());

   std::string strTmp;
   strTmp = ini.GetValue("tests", "http-proxy", "");
   std::transform(strTmp.begin(), strTmp.end(), strTmp.begin(), ::toupper);
   HTTP_PROXY_TEST_ENABLED = (strTmp == "YES") ? true : false;
   PROXY_SERVER = ini.GetValue("http-proxy", "host", "");
   PROXY_SERVER_FAKE = ini.GetValue("http-proxy", "host_invalid", "");

   // required when a build is generated with the macro DEBUG_CURL
   CURL_LOG_FOLDER = ini.GetValue("local", "curl_logs_folder", "");

   CERT_AUTH_FILE = ini.GetValue("local", "ca_file", "");
   SSL_CERT_FILE = ini.GetValue("local", "ssl_cert_file", "");
   SSL_KEY_FILE = ini.GetValue("local", "ssl_key_file", "");
   SSL_KEY_PWD = ini.GetValue("local", "ssl_key_pwd", "");


   if (HTTP_PROXY_TEST_ENABLED && (PROXY_SERVER.empty() || PROXY_SERVER_FAKE.empty()))
   {
      std::clog << "[ERROR] Check your INI file parameters."
         " Disable tests that don't have a server/port value."
         << std::endl;
      return false;
   }

   return true;
}

void GlobalTestCleanUp(void)
{

   return;
}

void TimeStampTest(std::ostringstream& ssTimestamp)
{
   time_t tRawTime;
   tm * tmTimeInfo;
   time(&tRawTime);
   tmTimeInfo = localtime(&tRawTime);

   ssTimestamp << (tmTimeInfo->tm_year) + 1900
      << "/" << tmTimeInfo->tm_mon + 1 << "/" << tmTimeInfo->tm_mday << " at "
      << tmTimeInfo->tm_hour << ":" << tmTimeInfo->tm_min << ":" << tmTimeInfo->tm_sec;
}

int TestProgressCallback(void* ptr, double dTotalToDownload, double dNowDownloaded,
                         double dTotalToUpload, double dNowUploaded)
{
   // ensure that the file to be downloaded is not empty
   // because that would cause a division by zero error later on
   if (dTotalToDownload <= 0.0)
      return 0;

   // how wide you want the progress meter to be
   const int iTotalDots = 20;
   double dFractionDownloaded = dNowDownloaded / dTotalToDownload;
   // part of the progressmeter that's already "full"
   int iDots = round(dFractionDownloaded * iTotalDots);

   // create the "meter"
   int iDot = 0;
   std::cout << static_cast<unsigned>(dFractionDownloaded * 100) << "% [";

   // part  that's full already
   for (; iDot < iDots; iDot++)
      std::cout << "=";

   // remaining part (spaces)
   for (; iDot < iTotalDots; iDot++)
      std::cout << " ";

   // and back to line begin - do not forget the fflush to avoid output buffering problems!
   std::cout << "]           \r" << std::flush;

   // if you don't return 0, the transfer will be aborted - see the documentation
   return 0;
}

bool GetFileTime(const char* const & pszFilePath, time_t& tLastModificationTime)
{
   FILE* pFile = fopen(pszFilePath, "rb");

   if (pFile != nullptr)
   {
      struct stat file_info;

#ifndef LINUX
      if (fstat(_fileno(pFile), &file_info) == 0)
#else
      if (fstat(fileno(pFile), &file_info) == 0)
#endif
      {
         tLastModificationTime = file_info.st_mtime;
         return true;
      }
   }

   return false;
}

#ifdef BOOST
bool AreFilesEqual(const std::string& strlFilePath, const std::string& strrFilePath)
{
   io::mapped_file_source f1(strlFilePath);
   io::mapped_file_source f2(strrFilePath);

   if (f1.size() == f2.size() &&
      std::equal(f1.data(), f1.data() + f1.size(), f2.data()))
      return true; // The files are equal
   else
      return false; //The files are not equal
}
#else
#define BUFFER_SIZE 0xFFF // 4KBytes... small files...
bool AreFilesEqual(const std::string& strlFilePath, const std::string& strrFilePath)
{
   std::ifstream lFile(strlFilePath.c_str(), std::ifstream::in | std::ifstream::binary);
   std::ifstream rFile(strrFilePath.c_str(), std::ifstream::in | std::ifstream::binary);

   if (!lFile.is_open() || !rFile.is_open())
   {
      return false;
   }

#ifndef USE_SMART_PTR
   char *lBuffer = new char[BUFFER_SIZE]();
   char *rBuffer = new char[BUFFER_SIZE]();
#else
   std::unique_ptr<char[]> lBuffer(new char[BUFFER_SIZE]);
   std::unique_ptr<char[]> rBuffer(new char[BUFFER_SIZE]);
#endif

   do
   {
#ifndef USE_SMART_PTR
      lFile.read(lBuffer, BUFFER_SIZE);
      rFile.read(rBuffer, BUFFER_SIZE);
#else
      lFile.read(lBuffer.get(), BUFFER_SIZE);
      rFile.read(rBuffer.get(), BUFFER_SIZE);
#endif

#ifndef USE_SMART_PTR
      if (std::memcmp(lBuffer, rBuffer, BUFFER_SIZE) != 0)
#else
      if (std::memcmp(lBuffer.get(), rBuffer.get(), BUFFER_SIZE) != 0)
#endif
      {
#ifndef USE_SMART_PTR
         delete[] lBuffer;
         delete[] rBuffer;
#endif

         return false;
      }
   } while (lFile.good() || rFile.good());

#ifndef USE_SMART_PTR
   delete[] lBuffer;
   delete[] rBuffer;
#endif

   return true;
}
#endif
