project(Dog)
cmake_minimum_required(VERSION 3.2)

add_definitions("-Wall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_BUILD_TYPE Debug)

if (CMAKE_BUILD_TYPE EQUAL RelWithDebInfo)
	set(MY_DEBUG 1)
elseif (CMAKE_BUILD_TYPE EQUAL Debug)
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-inline -O0")
else()
	set(MY_DEBUG 0)
endif()

if (ASAN EQUAL 1)
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-inline -O0 -fsanitize=address,undefined")
	MESSAGE("ASAN mode")
endif()

set(DOG_VERSION 2.5)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERSION=\"\\\"${DOG_VERSION}\\\"\" -DDEBUG=${MY_DEBUG}")

if (CMAKE_BUILD_TYPE EQUAL RelWithDebInfo)
	cmake_policy(SET CMP0069 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_executable(
  Dog
  ../book.cpp
  ../eval.cpp
  ../eval_par.cpp
  ../main.cpp
  ../max.cpp
  ../max-ascii.cpp
  ../psq.cpp
  ../search.cpp
  ../stats.cpp
  ../str.cpp
  ../syzygy.cpp
  ../test.cpp
  ../tt.cpp
  ../tui.cpp
  ../tuner.cpp
  ../usb-device.cpp
  ../fathom/src/tbprobe.c
)

include(FindPkgConfig)

find_package(OpenMP REQUIRED)
if (ASAN EQUAL 1)
	target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX -fsanitize=address,undefined)
else()
	target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX)
endif()

target_include_directories(Dog PUBLIC ../../include ../fathom/src)

set_target_properties(Dog PROPERTIES OUTPUT_NAME Dog)
