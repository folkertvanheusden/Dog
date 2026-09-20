cmake_minimum_required(VERSION 3.31)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(Dog)

idf_build_set_property(MINIMAL_BUILD ON)
