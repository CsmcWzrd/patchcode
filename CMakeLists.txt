cmake_minimum_required (VERSION 3.10)
project (patchcode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories ("${CMAKE_SOURCE_DIR}/src")
add_subdirectory (src)
