project(Dog)
cmake_minimum_required(VERSION 3.2)

add_definitions("-Wall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
#set(CMAKE_BUILD_TYPE Debug)

if (CMAKE_BUILD_TYPE EQUAL RelWithDebInfo)
	set(MY_DEBUG 1)
elseif (CMAKE_BUILD_TYPE EQUAL Debug)
	set(MY_DEBUG 1)
else()
	set(MY_DEBUG 0)
endif()

set(DOG_VERSION 2.1)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERSION=\"\\\"${DOG_VERSION}\\\"\" -Ofast -DDEBUG=${MY_DEBUG}")

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

add_executable(
  Dog
  ../eval.cpp
  ../eval_par.cpp
  ../main.cpp
  ../max.cpp
  ../psq.cpp
  ../syzygy.cpp
  ../tt.cpp
  ../fathom/src/tbprobe.c
)

include(FindPkgConfig)

find_package(OpenMP REQUIRED)
target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX)

target_include_directories(Dog PUBLIC ../../include ../fathom/src)

set_target_properties(Dog PROPERTIES OUTPUT_NAME Dog)
