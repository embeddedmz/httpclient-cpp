cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib)

add_definitions(-DLINUX)

include_directories(HTTP)

add_subdirectory(HTTP)
add_subdirectory(TestHTTP)

enable_testing ()

# test if the test INI file exist, otherwise default it to the one in TestHTTP folder
IF (NOT TEST_INI_FILE)
    SET(TEST_INI_FILE "./TestHTTP/template_test_conf.ini")
    MESSAGE(WARNING "You didn't provide an INI test configuration file.\
    Defaulting TEST_INI_FILE to ./TestHTTP/template_test_conf.ini")
ENDIF()

add_test (NAME HttpClientTest COMMAND test_httpclient ${TEST_INI_FILE})
