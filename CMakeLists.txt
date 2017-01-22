cmake_minimum_required(VERSION 2.6)

# Project configuration
project(HTTPClient)
set(LIBRARY_OUTPUT_PATH lib/${CMAKE_BUILD_TYPE})
set(CMAKE_CXX_STANDARD 14)

add_definitions(-DLINUX)

# Locate libcURL
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

include_directories(HTTP)

file(GLOB_RECURSE source_files HTTP/*)
add_library(httpclient STATIC ${source_files})
