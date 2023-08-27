cmake_minimum_required(VERSION 3.14)
project(shm-sequencer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# dependencies

# include(CTest)
# enable_testing()

set(CMAKE_THREAD_PREFER_PTHREAD ON)
find_package(Threads REQUIRED)

set(BOOST_ROOT $ENV{HOME}/local/boost-installed)
find_package(Boost 1.83.0 REQUIRED)

set(FLATBUF_ROOT $ENV{HOME}/local/flatbuffers)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# FetchContent_Declare(
#   flatbuffers
#   URL https://github.com/google/flatbuffers/releases/download/v23.5.26/Linux.flatc.binary.g++-10.zip
#   DOWNLOAD_NO_EXTRACT OFF
# )
# FetchContent_MakeAvailable(flatbuffers)

include_directories(
  ${Boost_INCLUDE_DIR}
  ${FLATBUF_ROOT}/include
)

# find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
# if(CMAKE_CXX_CPPCHECK)
# 	list(APPEND CMAKE_CXX_CPPCHECK
#     "--enable=all"
#     "--suppress=missingIncludeSystem"
#     "--suppress=toomanyconfigs"
#   )
# endif()

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" "-checks=*,-fuchsia-*")

add_subdirectory(src)
