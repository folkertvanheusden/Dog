project(Dog)
cmake_minimum_required(VERSION 3.2)

add_definitions("-Wall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(DOG_VERSION 0.9)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERSION=\"\\\"${DOG_VERSION}\\\"\"")

#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
#set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

add_executable(
  Dog
  ../eval.cpp
  ../eval_par.cpp
  ../main.cpp
  ../psq.cpp
  ../tt.cpp
)

include(FindPkgConfig)

find_package(OpenMP REQUIRED)
target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX)

target_include_directories(Dog PUBLIC ../../include)

set_target_properties(Dog PROPERTIES OUTPUT_NAME Dog)
