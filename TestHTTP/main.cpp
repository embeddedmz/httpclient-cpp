#include "gtest/gtest.h"   // Google Test Framework
#include "test_utils.h"    // Helpers for tests

// Test subject (SUT)
#include "HTTPClient.h"

#define PRINT_LOG [](const std::string& strLogMsg) { std::cout << strLogMsg << std::endl;  }

// Test parameters
extern bool HTTP_PROXY_TEST_ENABLED;

extern std::string CURL_LOG_FOLDER;

extern std::string CERT_AUTH_FILE;
extern std::string SSL_CERT_FILE;
extern std::string SSL_KEY_FILE;
extern std::string SSL_KEY_PWD;

extern std::string PROXY_SERVER;
extern std::string PROXY_SERVER_FAKE;

extern std::mutex g_mtxConsoleMutex;

namespace
{
// Fixture for HTTP tests
class HTTPTest : public ::testing::Test
{
protected:
   std::unique_ptr<CHTTPClient> m_pHTTPClient;

   HTTPTest() : m_pHTTPClient(nullptr)
   {
      CHTTPClient::SetCertificateFile(CERT_AUTH_FILE);

      #ifdef DEBUG_CURL
      CHTTPClient::SetCurlTraceLogDirectory(CURL_LOG_FOLDER);
      #endif
   }

   virtual ~HTTPTest() { }

   virtual void SetUp()
   {
      m_pHTTPClient.reset(new CHTTPClient(PRINT_LOG));
      
      /* to enable HTTPS, use the proper scheme in the URI 
       * or use SetHTTPS(true) */
      m_pHTTPClient->InitSession();
   }

   virtual void TearDown()
   {
      if (m_pHTTPClient.get() != nullptr)
      {
         m_pHTTPClient->CleanupSession();
         m_pHTTPClient.reset();
      }
   }
};

   // Unit tests

// Tests without a fixture (testing setters/getters and init./cleanup session)
TEST(HTTPClient, TestSession)
{
   CHTTPClient HTTPClient(PRINT_LOG);

   /* befor init. a session */
   EXPECT_TRUE(HTTPClient.GetURL().empty());
   EXPECT_TRUE(HTTPClient.GetProxy().empty());

   EXPECT_TRUE(HTTPClient.GetSSLCertFile().empty());
   EXPECT_TRUE(HTTPClient.GetSSLKeyFile().empty());
   EXPECT_TRUE(HTTPClient.GetSSLKeyPwd().empty());
   
   EXPECT_FALSE(HTTPClient.GetNoSignal());
   EXPECT_FALSE(HTTPClient.GetHTTPS());
   
   EXPECT_EQ(0, HTTPClient.GetTimeout());

   EXPECT_TRUE(HTTPClient.GetCurlPointer() == nullptr);

   EXPECT_EQ(CHTTPClient::SettingsFlag::ALL_FLAGS, HTTPClient.GetSettingsFlags());

   /* after init. a session */
   ASSERT_TRUE(HTTPClient.InitSession(true, CHTTPClient::ENABLE_LOG));

   // check that the parameters provided to InitSession
   EXPECT_EQ(CHTTPClient::ENABLE_LOG, HTTPClient.GetSettingsFlags());
   EXPECT_TRUE(HTTPClient.GetHTTPS());

   EXPECT_TRUE(HTTPClient.GetCurlPointer() != nullptr);

   HTTPClient.SetProxy("my_proxy");
   HTTPClient.SetSSLCertFile("file.cert");
   HTTPClient.SetSSLKeyFile("key.key");
   HTTPClient.SetSSLKeyPassword("passphrase");
   HTTPClient.SetTimeout(10);
   HTTPClient.SetHTTPS(false);
   HTTPClient.SetNoSignal(true);

   EXPECT_FALSE(HTTPClient.GetHTTPS());
   EXPECT_TRUE(HTTPClient.GetNoSignal());
   EXPECT_STREQ("http://my_proxy", HTTPClient.GetProxy().c_str());
   EXPECT_STREQ("file.cert", HTTPClient.GetSSLCertFile().c_str());
   EXPECT_STREQ("key.key", HTTPClient.GetSSLKeyFile().c_str());
   EXPECT_STREQ("passphrase", HTTPClient.GetSSLKeyPwd().c_str());
   EXPECT_EQ(10, HTTPClient.GetTimeout());

   HTTPClient.SetProgressFnCallback(reinterpret_cast<void*>(0xFFFFFFFF), &TestProgressCallback);
   EXPECT_EQ(&TestProgressCallback, *HTTPClient.GetProgressFnCallback());
   EXPECT_EQ(reinterpret_cast<void*>(0xFFFFFFFF), HTTPClient.GetProgressFnCallbackOwner());

   EXPECT_TRUE(HTTPClient.CleanupSession());
}

TEST(HTTPClient, TestDoubleInitializingSession)
{
   CHTTPClient HTTPClient(PRINT_LOG);

   ASSERT_TRUE(HTTPClient.InitSession());
   EXPECT_FALSE(HTTPClient.InitSession());
   EXPECT_TRUE(HTTPClient.CleanupSession());
}

TEST(HTTPClient, TestDoubleCleanUp)
{
   CHTTPClient HTTPClient(PRINT_LOG);

   ASSERT_TRUE(HTTPClient.InitSession());
   EXPECT_TRUE(HTTPClient.CleanupSession());
   EXPECT_FALSE(HTTPClient.CleanupSession());
}

TEST(HTTPClient, TestCleanUpWithoutInit)
{
   CHTTPClient HTTPClient(PRINT_LOG);

   ASSERT_FALSE(HTTPClient.CleanupSession());
}

TEST(HTTPClient, TestMultithreading)
{
   const char* arrDataArray[3] = { "Thread 1", "Thread 2", "Thread 3" };
   unsigned uInitialCount = CHTTPClient::GetCurlSessionCount();

   auto ThreadFunction = [](const char* pszThreadName)
   {
      CHTTPClient HTTPClient([](const std::string& strMsg) { std::cout << strMsg << std::endl; });
      if (pszThreadName != nullptr)
      {
         g_mtxConsoleMutex.lock();
         std::cout << pszThreadName << std::endl;
         g_mtxConsoleMutex.unlock();
      }
   };

   std::thread FirstThread(ThreadFunction, arrDataArray[0]);
   std::thread SecondThread(ThreadFunction, arrDataArray[1]);
   std::thread ThirdThread(ThreadFunction, arrDataArray[2]);

   // synchronize threads
   FirstThread.join();                 // pauses until first finishes
   SecondThread.join();                // pauses until second finishes
   ThirdThread.join();                 // pauses until third finishes

   ASSERT_EQ(uInitialCount, CHTTPClient::GetCurlSessionCount());
}

// HTTP tests using a fixture

TEST_F(HTTPTest, TestGetPage)
{
   std::string strWebPage;
   long lWebPageCode = 0;
   
   m_pHTTPClient->SetHTTPS(true);

   EXPECT_TRUE(m_pHTTPClient->GetText("www.google.com", strWebPage, lWebPageCode));
   EXPECT_FALSE(strWebPage.empty());
   EXPECT_EQ(200, lWebPageCode);
}

TEST_F(HTTPTest, TestDownloadFile)
{
   long lHTTPSCode = 0;

   // to display a beautiful progress bar on console
   m_pHTTPClient->SetProgressFnCallback(nullptr, &TestProgressCallback);

   ASSERT_TRUE(m_pHTTPClient->DownloadFile("test.pem", "https://curl.haxx.se/ca/cacert.pem", lHTTPSCode));
   /* to properly show the progress bar */
   std::cout << std::endl;

   /* TODO : we can check the SHA1 of the downloaded file with a value provided in the INI file */

   EXPECT_EQ(200, lHTTPSCode);

   /* delete test file */
   EXPECT_TRUE(remove("test.pem") == 0);
}

// check for failure: inexistant file
TEST_F(HTTPTest, TestDownloadInexistentFile)
{
   long lResultHTTPCode = 0;

   EXPECT_TRUE(m_pHTTPClient->DownloadFile("test.txt", "https://curl.haxx.se/ca/inexistent_file.txt", lResultHTTPCode));
   EXPECT_EQ(404, lResultHTTPCode);
}

TEST_F(HTTPTest, TestUploadForm)
{
   long lResultHTTPCode = 0;

   // generating a dummy file name with a timestamp
   std::ostringstream fileName;
   time_t rawtime;
   tm * timeinfo;
   time(&rawtime);
   timeinfo = localtime(&rawtime);

   fileName << "TestPostForm_" << (timeinfo->tm_year) + 1900 << "_" << timeinfo->tm_mon + 1
      << "_" << timeinfo->tm_mday << "-" << timeinfo->tm_hour
      << "_" << timeinfo->tm_min << "_" << timeinfo->tm_sec << ".txt";

   // creating a dummy file to upload via a post form request
   std::ofstream ofDummyFile(fileName.str().c_str());
   ASSERT_TRUE(static_cast<bool>(ofDummyFile));
   ofDummyFile << "Dummy file for the unit test 'TestUploadForm' of the httpclient-cpp Project.";
   ASSERT_TRUE(static_cast<bool>(ofDummyFile));
   ofDummyFile.close();

   /* Filling information about the form in a PostFormInfo object */
   CHTTPClient::PostFormInfo UploadInfo;

   UploadInfo.AddFormFile("submitted", fileName.str());
   UploadInfo.AddFormContent("filename", fileName.str());

   /* The details of the upload of the dummy test file can be found under
   http://posttestserver.com/data/[year]/[month]/[day]/restclientcpptests/ */
   m_pHTTPClient->UploadForm("http://posttestserver.com/post.php?dir=restclientcpptests",
               UploadInfo, lResultHTTPCode);

   EXPECT_EQ(200, lResultHTTPCode);

   // remove dummy file
   remove(fileName.str().c_str());
}

// Proxy Test with GET command
TEST_F(HTTPTest, TestProxy)
{
   if (HTTP_PROXY_TEST_ENABLED)
   {
      std::string strWebPage;
      long lWebPageCode = 0;

      // Proxy setup
      m_pHTTPClient->SetProxy(PROXY_SERVER);

      EXPECT_TRUE(m_pHTTPClient->GetText("https://www.google.com", strWebPage, lWebPageCode));
      EXPECT_FALSE(strWebPage.empty());
      EXPECT_EQ(200, lWebPageCode);
   }
   else
      std::cout << "HTTP Proxy tests are disabled !" << std::endl;
}

// check for failure
TEST_F(HTTPTest, TestInexistantProxy)
{
   if (HTTP_PROXY_TEST_ENABLED)
   {
      std::string strWebPage;
      long lWebPageCode = 0;

      // Proxy setup - non existent address
      m_pHTTPClient->SetProxy(PROXY_SERVER_FAKE);
      m_pHTTPClient->SetHTTPS(true);

      EXPECT_FALSE(m_pHTTPClient->GetText("https://www.google.com", strWebPage, lWebPageCode));
      EXPECT_TRUE(strWebPage.empty());
      EXPECT_EQ(0, lWebPageCode);
   }
   else
      std::cout << "HTTP Proxy tests are disabled !" << std::endl;
}

} // namespace

int main(int argc, char **argv)
{
   if (argc > 1 && GlobalTestInit(argv[1])) // loading test parameters from the INI file...
   {
      ::testing::InitGoogleTest(&argc, argv);
      int iTestResults = RUN_ALL_TESTS();

      ::GlobalTestCleanUp();

      return iTestResults;
   }
   else
   {
      std::cerr << "[ERROR] Encountered an error while loading test parameters from provided INI file !" << std::endl;
      return 1;
   }
}
