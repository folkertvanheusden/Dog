cmake_minimum_required(VERSION 3.31)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PROJECT_VERSION_MAJOR 3)
set(PROJECT_VERSION_MINOR 1)
set(DOG_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDOG_VERSION=\"\\\"${DOG_VERSION}\\\"\" -DESP32 -DESP32_S3 -fexceptions")

project(esp32chesstest)

idf_build_set_property(MINIMAL_BUILD ON)
