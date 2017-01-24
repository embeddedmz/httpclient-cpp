# HTTP client for C++
[![MIT license](https://img.shields.io/badge/license-MIT-blue.svg)](http://opensource.org/licenses/MIT)


## About
This is a simple HTTP client for C++. It wraps libcurl for HTTP requests and meant to be a portable
and easy-to-use API to perform HTTP related operations.

Compilation has been tested with:
- GCC 5.4.0 (GNU/Linux Ubuntu 16.04 LTS)
- Microsoft Visual Studio 2015 (Windows 10)

Underlying libraries:
- [libcurl](http://curl.haxx.se/libcurl/)

## Usage
Create an object and provide to its constructor a callable object (for log printing) having this signature :

```cpp
void(const std::string&)
```

Later, you can disable log printing by avoiding the flag CHTTPClient::SettingsFlag::ENABLE_LOG when initializing a session.

```cpp
#include "HTTPClient.h"

CHTTPClient HTTPClient([](const std::string&){ std::cout << strLogMsg << std::endl; });
```

If you want to send requests to an HTTPS server, you must provide a certificate authority (CA) file.
You can download one from : https://curl.haxx.se/ca/cacert.pem
and set it's local path, once for all, before sending HTTPS requests :

```cpp
CHTTPClient::SetCertificateFile("C:\\cacert.pem");
```

Before performing one or more requests, a session must be initialized. If the URI provided in the methods below
is not prefixed with the protocol scheme (http:// or https://), you have to set the first parameter of InitSession
to true to enable HTTPS, as by default, this parameter is set to false. You can also choose to disable log printing
by setting the second parameter to a proper value.

```cpp
/* HTTPS disabled if the protocol scheme is not mentionned in the URIs
 * Log printing, peer and host verification are enabled for HTTPS URIs are enabled by default */
HTTPClient.InitSession();
```
To download a file :

```cpp
long lResultHTTPCode = 0;

HTTPClient.DownloadFile("test.pem", "https://curl.haxx.se/ca/cacert.pem", lHTTPSCode);

/* lResultHTTPCode should be equal to 200 if the request is successfully processed */
```

To upload a file via a POST Form :

```cpp
long lResultHTTPCode = 0;

/* Filling information about the form in a PostFormInfo object */
CHTTPClient::PostFormInfo UploadInfo;

UploadInfo.AddFormFile("submitted", fileName.str());
UploadInfo.AddFormContent("filename", fileName.str());

/* The details of the upload of the dummy test file can be found under
http://posttestserver.com/data/[year]/[month]/[day]/restclientcpptests/ */
HTTPClient.UploadForm("http://posttestserver.com/post.php?dir=restclientcpptests",
                      UploadInfo,
                      lResultHTTPCode);

/* lResultHTTPCode should be equal to 200 if the request is successfully processed */
```

To send a GET request to a web page and save its content in a string:

```cpp
std::string strWebPage;
long lWebPageCode = 0;

HTTPClient.GetText("https://www.google.com", strWebPage, lWebPageCode);

/* lWebPageCode should be equal to 200 if the request is successfully executed */
```

You can also set parameters such as the time out (in seconds), the HTTP proxy server etc... before sending
your request.

To create and remove a remote empty directory :

```cpp
/* creates a directory "bookmarks" under http://127.0.0.1:21/documents/ */
HTTPClient.
```

Always check that the methods above returns true, otherwise, that means that  the request wasn't properly
executed.

Finally cleanup can be done automatically if the object goes out of scope. If the log printing is enabled,
a warning message will be printed. Or you can cleanup manually by calling CleanupSession() :

```cpp
HTTPClient.CleanupSession();
```

After cleaning the session, if you want to reuse the object, you need to re-initialize it with the
proper method.

## Callback to a Progress Function

A pointer to a callback progress meter function or a callable object (lambda, functor etc...), which should match the prototype shown below, can be passed.

```cpp
int ProgCallback(void* ptr, double dTotalToDownload, double dNowDownloaded, double dTotalToUpload, double dNowUploaded);
```

This function gets called by libcurl instead of its internal equivalent with a frequent interval. While data is being transferred it will be called very frequently, and during slow periods like when nothing is being transferred it can slow down to about one call per second.

Returning a non-zero value from this callback will cause libcurl to abort the transfer.

The unit test "TestDownloadFile" demonstrates how to use a progress function to display a progress bar on console when downloading a file.

## Thread Safety

A mutex is used to increment/decrement atomically the count of CHTTPClient objects.

`curl_global_init` is called when the count reaches one and `curl_global_cleanup` is called when the counter
become zero.

Do not share CHTTPClient objects across threads as this would mean accessing libcurl handles from multiple threads
at the same time which is not allowed.

The method SetNoSignal can be set to skip all signal handling. This is important in multi-threaded applications as DNS
resolution timeouts use signals. The signal handlers quite readily get executed on other threads.

## HTTP Proxy Tunneling Support

An HTTP Proxy can be set to use for the upcoming request.
To specify a port number, append :[port] to the end of the host name. If not specified, `libcurl` will default to using port 1080 for proxies. The proxy string may be prefixed with `http://` or `https://`. If no HTTP(S) scheme is specified, the address provided to `libcurl` will be prefixed with `http://` to specify an HTTP proxy. A proxy host string can embedded user + password.
The operation will be tunneled through the proxy as curl option `CURLOPT_HTTPPROXYTUNNEL` is enabled by default.
A numerical IPv6 address must be written within [brackets].

```cpp
// set HTTP proxy address
HTTPClient.SetProxy("https://37.187.100.23:3128");

/* the following request will be tunneled through the proxy */
std::string strWebPage;
long lWebPageCode = 0;

HTTPClient.GetText("https://www.google.com", strWebPage, lWebPageCode);

/* lWebPageCode should be equal to 200 if the request is successfully executed */
```

## Installation
You will need CMake to generate a makefile for the static library to build the tests or the code coverage 
program.

Also make sure you have libcurl and Google Test installed.

You can follow this script https://gist.github.com/fideloper/f72997d2e2c9fbe66459 to install libcurl.

This tutorial will help you installing properly Google Test on Ubuntu: https://www.eriksmistad.no/getting-started-with-google-test-on-ubuntu/

The CMake script located in the tree will produce a makefile for the creation of a static library,
whereas the one under TestHTTP will produce the unit tests program.

To create a debug static library, change directory to the one containing the first CMakeLists.txt

```Shell
cmake . -DCMAKE_BUILD_TYPE:STRING=Debug
make
```

To create a release static library, just change "Debug" by "Release".

The library will be found under lib/[BUILD_TYPE]/libHTTPClient.a

For the unit tests program, first build the static library and use the same build type when
building it :

```Shell
cd TestHTTP/
cmake . -DCMAKE_BUILD_TYPE=Debug     # or Release
make
```

To run it, you must indicate the path of the INI conf file (see the section below)
```Shell
./bin/[BUILD_TYPE]/test_httpclient /path_to_your_ini_file/conf.ini
```

## Run Unit Tests

[simpleini](https://github.com/brofield/simpleini) is used to gather unit tests parameters from
an INI configuration file. You need to fill that file with some parameters.
You can also disable some tests (HTTP proxy for instance) and indicate
parameters only for the enabled tests. A template of the INI file exists already under TestHTTP/


Example : (Proxy tests are enbaled)

```ini
[tests]
http-proxy=yes

[http-proxy]
host=127.0.0.1:3128
host_invalid=127.0.0.1:6666
```

You can also generate an XML file of test results by adding this argument when calling the test program

```Shell
./bin/[BUILD_TYPE]/test_httpclient /path_to_your_ini_file/conf.ini --gtest_output="xml:./TestHTTP.xml"
```

## Memory Leak Check

Visual Leak Detector has been used to check memory leaks with the Windows build (Visual Sutdio 2015)
You can download it here: https://vld.codeplex.com/

To perform a leak check with the Linux build, you can do so :

```Shell
valgrind --leak-check=full ./bin/Debug/test_httpclient /path_to_ini_file/conf.ini
```

## Code Coverage

The code coverage build doesn't use the static library but compiles and uses directly the 
HTTPClient-C++ API in the test program.

First of all, in TestHTTP/CMakeLists.txt, find and repalce :
```
"/home/amzoughi/Test/http_github.ini"
```
by the location of your ini file and launch the code coverage :

```Shell
cd TestHTTP/
cmake . -DCMAKE_BUILD_TYPE=Coverage
make
make coverage_httpclient
```

If everything is OK, the results will be found under ./TestHTTP/coverage/index.html

Under Visual Studio, you can simply use OpenCppCoverage (https://opencppcoverage.codeplex.com/)

## Contribute
All contributions are highly appreciated. This includes updating documentation, writing code and unit tests
to increase code coverage and enhance tools.

Try to preserve the existing coding style (Hungarian notation, indentation etc...).

## Issues

### Compiling with the macro DEBUG_CURL

If you compile the test program with the preprocessor macro DEBUG_CURL, to enable curl debug informations,
the static library used must also be compiled with that macro. Don't forget to mention a path where to store
log files in the INI file if you want to use that feature in the unit test program (curl_logs_folder under [local])
