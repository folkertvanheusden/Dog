cmake_minimum_required(VERSION 3.31)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PROJECT_VERSION_MAJOR 3)
set(PROJECT_VERSION_MINOR 11)
set(DOG_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

execute_process(
    COMMAND echo `git describe --always --dirty --broken`_`git branch --show-current`
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REV_DOG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git describe --always --dirty --broken
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/include/libchess/
    OUTPUT_VARIABLE GIT_REV_LC
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git describe --always --dirty --broken
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/fathom
    OUTPUT_VARIABLE GIT_REV_FATHOM
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ESP32 with one of: WEMOS32, ESP32_S3_QTPY, ESP32_S3_XIAO, ESP32_S3_WAVESHARE
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDOG_VERSION=\"\\\"${DOG_VERSION}\\\"\" -DESP32 -DESP32_S3_XIAO -DGIT_REV_DOG=\"\\\"${GIT_REV_DOG}\\\"\" -DGIT_REV_LC=\"\\\"${GIT_REV_LC}\\\"\" -DGIT_REV_FATHOM=\"\\\"${GIT_REV_FATHOM}\\\"\" -DBUILD_TYPE=\"\\\"${CMAKE_BUILD_TYPE}\\\"\" -DBUILD_TARGET=\"\\\"regular\\\"\"")

project(esp32chesstest)

idf_build_set_property(MINIMAL_BUILD ON)
