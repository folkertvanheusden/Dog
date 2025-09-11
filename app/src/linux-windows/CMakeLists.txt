project(Dog)
cmake_minimum_required(VERSION 3.5)

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

if (GPROF EQUAL 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
	MESSAGE("WITH GPROF")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINSTRUMENTED=1")
else()
	MESSAGE("without GPROF")
endif()

if (ASAN EQUAL 1)
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
	MESSAGE("WITH ASAN")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINSTRUMENTED=1")
else()
	MESSAGE("without ASAN")
endif()

if (MSAN EQUAL 1)
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fsanitize-recover=all")
	MESSAGE("WITH MSAN")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINSTRUMENTED=1")
else()
	MESSAGE("without MSAN")
endif()

if (TSAN EQUAL 1)
	set(MY_DEBUG 1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
	MESSAGE("WITH TSAN")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINSTRUMENTED=1")
else()
	MESSAGE("without TSAN")
endif()

execute_process(
    COMMAND git describe --always --dirty --broken
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REV_DOG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
MESSAGE("DOG GIT revision: ${GIT_REV_DOG}")

execute_process(
    COMMAND git describe --always --dirty --broken
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../../include/libchess/
    OUTPUT_VARIABLE GIT_REV_LC
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
MESSAGE("libchess GIT revision: ${GIT_REV_LC}")

execute_process(
    COMMAND git describe --always --dirty --broken
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../../src/fathom
    OUTPUT_VARIABLE GIT_REV_FATHOM
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
MESSAGE("Fathom GIT revision: ${GIT_REV_FATHOM}")

execute_process(
    COMMAND git describe --always --dirty --broken
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../../book/chess-openings
    OUTPUT_VARIABLE GIT_REV_BOOK
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
MESSAGE("Book GIT revision: ${GIT_REV_BOOK}")

set(PROJECT_VERSION_MAJOR 3)
set(PROJECT_VERSION_MINOR 11)
set(DOG_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDOG_VERSION=\"\\\"${DOG_VERSION}\\\"\" -DDEBUG=${MY_DEBUG} -Wall -Wextra -pedantic -DBUILD_TYPE=\"\\\"${CMAKE_BUILD_TYPE}\\\"\"")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGIT_REV_DOG=\"\\\"${GIT_REV_DOG}\\\"\" -DGIT_REV_LC=\"\\\"${GIT_REV_LC}\\\"\" -DGIT_REV_FATHOM=\"\\\"${GIT_REV_FATHOM}\\\"\" -DGIT_REV_BOOK=\"\\\"${GIT_REV_BOOK}\\\"\"")

set_source_files_properties(../fathom/src/tbprobe.c PROPERTIES LANGUAGE CXX)

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	MESSAGE("WITH LTO")
	cmake_policy(SET CMP0069 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
	MESSAGE("without LTO")
endif()

IF (WIN32 OR MSYS OR MINGW OR MSVC)
	MESSAGE("Windows build")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-ms-bitfields")
ENDIF ()

set(APP_SOURCES
  ../book.cpp
  ../eval.cpp
  ../eval-stats.cpp
  ../main.cpp
  ../max.cpp
  ../max-ascii.cpp
  ../nnue.cpp
  ../san.cpp
  ../search.cpp
  ../state_exporter.cpp
  ../stats.cpp
  ../str.cpp
  ../syzygy.cpp
  ../test.cpp
  ../tt.cpp
  ../tui.cpp
  ../win32.cpp
  ../fathom/src/tbprobe.c
)

add_executable(
  Dog
  ${APP_SOURCES}
)
target_compile_options(Dog PRIVATE -DBUILD_TARGET=\"regular\")

add_executable(
  Dog-avx
  ${APP_SOURCES}
)

target_compile_options(Dog-avx PRIVATE -mavx2 -mbmi2 -mavx -mbmi -O3 -DBUILD_TARGET=\"AVX\")

add_executable(
  Dog-native
  ${APP_SOURCES}
)

target_compile_options(Dog-native PRIVATE -march=native -mtune=native -O3 -DBUILD_TARGET=\"native\")

add_executable(
    Dog-stats-prober
    ../emit_stats.cpp
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
		target_link_libraries(Dog PRIVATE -fsanitize=thread Threads::Threads)
	elseif (MSAN EQUAL 1)
		target_link_libraries(Dog PRIVATE -fsanitize=memory Threads::Threads)
	else()
		target_link_libraries(Dog PRIVATE Threads::Threads)
		target_link_libraries(Dog-avx PRIVATE Threads::Threads)
		target_link_libraries(Dog-native PRIVATE Threads::Threads)
	endif()
endif()

if (GPROF EQUAL 1)
	target_link_libraries(Dog PRIVATE -pg)
endif()

target_include_directories(Dog PUBLIC ../../include ../fathom/src)
target_include_directories(Dog-avx PUBLIC ../../include ../fathom/src)
target_include_directories(Dog-native PUBLIC ../../include ../fathom/src)
target_include_directories(Dog-stats-prober PUBLIC ../../include ../fathom/src)

set_target_properties(Dog PROPERTIES OUTPUT_NAME Dog)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION /usr/games)

SET(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_CONTACT "folkert@vanheusden.comm")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Folkert van Heusden")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(CPACK_DEBIAN_PACKAGE_VERSION ${DOG_VERSION})
set(CPACK_DEBIAN_PACKAGE_RELEASE ${DOG_VERSION})
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_COMPRESSION_TYPE zstd)
INCLUDE(CPack)
