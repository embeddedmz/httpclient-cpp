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

/* "submitted" is the name of the "file" input and "myfile.txt" is the location
of the file to submit.
<input type="file" name="submitted">
*/
UploadInfo.AddFormFile("submitted", "myfile.txt");

/* In some rare cases, some fields related to the form can be filled with
AddFormContent(), the 1st argument is the name of the input element and
the 2nd argument is the value assigned to it.
<input type="text" name="filename" value=""/>
*/
UploadInfo.AddFormContent("filename", "myfile.cpp");

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

There's also REST methods modeled after the most common HTTP commands:

```cpp
CHTTPClient::HeadersMap RequestHeaders;
CHTTPClient::HttpResponse ServerResponse;

// HEAD request
pRESTClient->Head("http://httpbin.org/get", RequestHeaders, ServerResponse);

// GET request
pRESTClient->Get("http://httpbin.org/get", RequestHeaders, ServerResponse);

// POST request
std::string strPostData = "data";
RequestHeaders.emplace("Content-Type", "text/text");
pRESTClient->Post("http://httpbin.org/post", RequestHeaders, strPostData, ServerResponse);

// PUT request
std::string strPutData = "data";
RequestHeaders.emplace("Content-Type", "text/text");

// 1. with a string
pRESTClient->Put("http://httpbin.org/put", RequestHeaders, strPutData, ServerResponse);

// 2. with a vector of char
CHTTPClient::ByteBuffer Buffer; // or std::vector<char>
Buffer.push_back('d');
Buffer.push_back('a');
Buffer.push_back('t');
Buffer.push_back('a');
pRESTClient->Put("http://httpbin.org/put", RequestHeaders, Buffer, ServerResponse)

// DELETE request
pRESTClient->Del("http://httpbin.org/delete", RequestHeaders, ServerResponse);

// Server's response
ServerResponse.iCode; // response's code
ServerResponse.mapHeaders; // response's headers
ServerResponse.strBody; // response's body
```

You can also set parameters such as the time out (in seconds), the HTTP proxy server etc... before sending
your request.

Always check that all the methods above return true, otherwise, that means that  the request wasn't properly
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

`curl_global_init` is called when the count of CHTTPClient objects equals to zero (when instanciating the first object).
`curl_global_cleanup` is called when the count of CHTTPClient objects become zero (when all CHTTPClient objects are destroyed).

Do not share CHTTPClient objects across threads as this would mean accessing libcurl handles from multiple threads
at the same time which is not allowed.

The method SetNoSignal can be used to skip all signal handling. This is important in multi-threaded applications as DNS
resolution timeouts use signals. The signal handlers quite readily get executed on other threads.

## HTTP Proxy Tunneling Support

An HTTP Proxy can be set to use for the upcoming request.
To specify a port number, append :[port] to the end of the host name. If not specified, `libcurl` will default to using port 1080 for proxies. The proxy string may be prefixed with `http://` or `https://`. If no HTTP(S) scheme is specified, the address provided to `libcurl` will be prefixed with `http://` to specify an HTTP proxy. A proxy host string can embed user + password.
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
You will need CMake to generate a makefile for the static library or to build the tests/code coverage program.

Also make sure you have libcurl and Google Test installed.

You can follow this script https://gist.github.com/fideloper/f72997d2e2c9fbe66459 to install libcurl.

This tutorial will help you installing properly Google Test on Ubuntu: https://www.eriksmistad.no/getting-started-with-google-test-on-ubuntu/

The CMake script located in the tree will produce Makefiles for the creation of the static library and for the unit tests program.

To create a debug static library and a test binary, change directory to the one containing the first CMakeLists.txt and :

```Shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE:STRING=Debug
make
```

To create a release static library, just change "Debug" by "Release".

The library will be found under "build/[Debug|Release]/lib/libhttpclient.a" whereas the test program will be located in "build/[Debug|Release]/bin/test_httpclient"

To directly run the unit test binary, you must indicate the path of the INI conf file (see the section below)
```Shell
./[Debug|Release]/bin/test_httpclient /path_to_your_ini_file/conf.ini
```

### Building under Windows via Visual Studio

This can be enhanced in future...

First of all, build libcurl using this fork of build-libcurl-windows : https://github.com/ribtoks/build-libcurl-windows

In fact, this fork will allow you to build libcurl under Visual Studio 2017.

```Shell
git clone https://github.com/ribtoks/build-libcurl-windows.git
```

Then just build libcurl using :

```Shell
build.bat
```

This batch script will automatically download the latest libcurl source code and build it using the most recent Visual Studio compiler
that it will find on your computer. For a particular version, you can modify the batch script...

Under YOUR_DIRECTORY\build-libcurl-windows\third-party\libcurl, you will find the curl include directory and a lib directory containing different type of libraries : dynamic and static x86 and x64 libraries compiled in Debug and Release mode (8 libraries). Later, we will be using the libraries located in lib\dll-debug-x64 and lib\dll-release-x64 as an example.

Concerning Google Test, the library will be downloaded and built automatically from its github repository. Someone with enough free time can do the same for libcurl and submit a pull request...

Download and install the latest version of CMake : https://cmake.org/download/ (e.g. Windows win64-x64 Installer)

Open CMake (cmake-gui)

In "Where is the source code", put the httpclient-cpp path (e.g. C:/Users/Amine/Documents/Work/PROJECTS/GitHub/httpclient-cpp), where the main CMakeLists.txt file exist.

In "Where to build binaries", paste the directory where you want to build the project (e.g. C:/Users/Amine/Documents/Work/PROJECTS/GitHub/httpclient_build)

Click on "Configure". After some time, an error message will be shown, that's normal as CMake is unable to find libcurl.

Make sure "Advanced" checkbox is checked, in the list, locate the variables prefixed with "CURL_" and update them with the path of libcurl include directory and libraries, for example :

CURL_INCLUDE_DIR C:\LIBS\build-libcurl-windows\third-party\libcurl\include
CURL_LIBRARY_DEBUG C:\LIBS\build-libcurl-windows\third-party\libcurl\lib\dll-debug-x64\libcurl_debug.lib
CURL_LIBRARY_RELEASE C:\LIBS\build-libcurl-windows\third-party\libcurl\lib\dll-release-x64\libcurl.lib

Click on "Configure" again ! You should not have errors this time. You can ignore the warning related to the test configuration file.

Then click on "Generate", you can choose a Visual Studio version if it is not done before (e.g. Visual Studio 15 2017 Win64, I think it should be the same version used to build libcurl...)

Finally, click on "Open Project" to open the solution in Visual Studio.

In Visual Studio, you can change the build type (Debug -> Release). Build the solution (press F7). It must succeed without any errors. You can close Visual Studio.

The library will be found under C:\Users\Amine\Documents\Work\PROJECTS\GitHub\httpclient_build\lib\Release\httpclient.lib

After building a program using "hhtpclient.lib", do not forget to copy libcurl DLL in the directory where the program binary is located.

For example, in the build directory (e.g. C:\Users\Amine\Documents\Work\PROJECTS\GitHub\httpclient_build), under "bin", directory, you may find "Debug", "Release" or both according to the build type used during the build in Visual Studio, and in it, the test program "test_httpclient.exe". Before executing it, make sure to copy the libcurl DLL in the same directory (e.g. copy C:\LIBS\build-libcurl-windows\third-party\libcurl\lib\dll-release-x64\libcurl.dll and the PDB file too if you want, do not change the name of the DLL !) The type of the library MUST correspond to the type of the .lib file fed to CMake-gui !

If you want to run the test program, in the command line, launch http_httpclient.exe with the path of you test configuration file (INI) :

```Shell
C:\Users\Amine\Documents\Work\PROJECTS\GitHub\httpclient_build\bin\[Debug | Release]\httpclient.exe PATH_TO_YOUR_TEST_CONF_FILE\conf.ini
```

## Run Unit Tests

[simpleini](https://github.com/brofield/simpleini) is used to gather unit tests parameters from
an INI configuration file. You need to fill that file with some parameters.
You can also disable some tests (HTTP proxy for instance) and indicate
parameters only for the enabled tests. A template of the INI file already exists under TestHTTP/


Example : (Proxy tests are enbaled)

```ini
[tests]
http-proxy=yes

[http-proxy]
host=127.0.0.1:3128
host_invalid=127.0.0.1:6666
```

You can also generate an XML file of test results by adding --getst_output argument when calling the test program

```Shell
./[Debug|Release]/bin/test_httpclient /path_to_your_ini_file/conf.ini --gtest_output="xml:./TestHTTP.xml"
```

An alternative way to compile and run unit tests :

```Shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DTEST_INI_FILE="full_or_relative_path_to_your_test_conf.ini"
make
make test
```

You may use a tool like https://github.com/adarmalik/gtest2html to convert your XML test result in an HTML file.

## Memory Leak Check

Visual Leak Detector has been used to check memory leaks with the Windows build (Visual Sutdio 2015)
You can download it here: https://vld.codeplex.com/

To perform a leak check with the Linux build, you can do so :

```Shell
valgrind --leak-check=full ./Debug/bin/test_httpclient /path_to_ini_file/conf.ini
```

## Code Coverage

The code coverage build doesn't use the static library but compiles and uses directly the 
HTTP Client API in the test program.

```Shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Coverage -DCOVERAGE_INI_FILE:STRING="full_path_to_your_test_conf.ini"
make
make coverage_httpclient
```

If everything is OK, the results will be found under ./TestHTTP/coverage/index.html

Make sure you feed CMake with a full path to your test conf INI file, otherwise, the coverage test
will be useless.

Under Visual Studio, you can simply use OpenCppCoverage (https://opencppcoverage.codeplex.com/)

## CppCheck Compliancy

The C++ code of the HTTPClient class is 100% Cppcheck compliant.

## Contribute
All contributions are highly appreciated. This includes updating documentation, writing code and unit tests
to increase code coverage and enhance tools.

Try to preserve the existing coding style (Hungarian notation, indentation etc...).

## Issues

### Compiling with the macro DEBUG_CURL

If you compile the test program with the preprocessor macro DEBUG_CURL, to enable curl debug informations,
the static library used must also be compiled with that macro. Don't forget to mention a path where to store
log files in the INI file if you want to use that feature in the unit test program (curl_logs_folder under [local])
