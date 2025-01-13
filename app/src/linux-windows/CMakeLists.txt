project(Dog)
cmake_minimum_required(VERSION 3.2)

add_definitions("-Wall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
#set(CMAKE_BUILD_TYPE Debug)

SET(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	MESSAGE("Release mode with debug info")
	set(MY_DEBUG 1)
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
	MESSAGE(" * DEBUG MODE * ")
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-inline -O0")
else()
	set(MY_DEBUG 0)
endif()

if (ASAN EQUAL 1)
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
	MESSAGE("WITH ASAN")
else()
	MESSAGE("without ASAN")
endif()

if (MSAN EQUAL 1)
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fsanitize-recover=all")
	MESSAGE("WITH MSAN")
else()
	MESSAGE("without MSAN")
endif()

if (TSAN EQUAL 1)
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
	MESSAGE("WITH TSAN")
else()
	MESSAGE("without TSAN")
endif()

set(PROJECT_VERSION_MAJOR 2)
set(PROJECT_VERSION_MINOR 9)
set(DOG_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDOG_VERSION=\"\\\"${DOG_VERSION}\\\"\" -DDEBUG=${MY_DEBUG} -Wall -Wextra -pedantic")

set_source_files_properties(../fathom/src/tbprobe.c PROPERTIES LANGUAGE CXX)

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	MESSAGE("WITH LTO")
	cmake_policy(SET CMP0069 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
	MESSAGE("without LTO")
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
  ../san.cpp
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

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)

include(FindPkgConfig)

find_package(OpenMP)
IF (OpenMP_FOUND)
	MESSAGE("WITH OpenMP")
        if (ASAN EQUAL 1)
                target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX -fsanitize=address,undefined Threads::Threads)
	elseif (TSAN EQUAL 1)
                target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX -fsanitize=thread Threads::Threads)
	elseif (MSAN EQUAL 1)
                target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX -fsanitize=memory Threads::Threads)
        else()
                target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX Threads::Threads)
        endif()
else()
	MESSAGE("without OpenMP")
	if (ASAN EQUAL 1)
		target_link_libraries(Dog PRIVATE -fsanitize=address,undefined Threads::Threads)
	elseif (TSAN EQUAL 1)
		target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX -fsanitize=thread Threads::Threads)
	elseif (MSAN EQUAL 1)
		target_link_libraries(Dog PRIVATE OpenMP::OpenMP_CXX -fsanitize=memory Threads::Threads)
	else()
		target_link_libraries(Dog PRIVATE Threads::Threads)
	endif()
endif()

target_include_directories(Dog PUBLIC ../../include ../fathom/src)

set_target_properties(Dog PROPERTIES OUTPUT_NAME Dog)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION /usr/games)

SET(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_CONTACT "mail@vanheusden.comm")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Folkert van Heusden")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(CPACK_DEBIAN_PACKAGE_VERSION ${DOG_VERSION})
set(CPACK_DEBIAN_PACKAGE_RELEASE ${DOG_VERSION})
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_COMPRESSION_TYPE zstd)
INCLUDE(CPack)
