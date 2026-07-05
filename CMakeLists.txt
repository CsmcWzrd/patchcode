cmake_minimum_required(VERSION 3.10)

project(patchcode
    VERSION 1.0.0
    DESCRIPTION "patchcode patch applier and diffcode patch generator"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src)

enable_testing()
add_test(NAME diffcode_smoke_tests
    COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:patchcode>:$ENV{PATH}"
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_diffcode_tests.sh)
