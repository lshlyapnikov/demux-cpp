cmake_minimum_required(VERSION 3.14)
project(shm-sequencer VERSION 0.1.0 LANGUAGES CXX)

# Uncomment this for verbose CMake output
# set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc--clang
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Werror")

#
# tools
#

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message("Found ccache: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

find_program(
  CLANG_TIDY_EXE 
  NAMES "clang-tidy" REQUIRED
)
set(
  CLANG_TIDY_COMMAND
  "${CLANG_TIDY_EXE}"
)

#
# dependencies
#

set(CMAKE_THREAD_PREFER_PTHREAD ON)
find_package(Threads REQUIRED)

set(BOOST_ROOT $ENV{HOME}/local/boost-installed)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.83.0 REQUIRED COMPONENTS log)

set(FLATBUF_ROOT $ENV{HOME}/local/flatbuffers)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(
  ${Boost_INCLUDE_DIR}
  ${FLATBUF_ROOT}/include
)

#
# artifacts
#

add_library(
  sequencer
  OBJECT src/sequencer.cpp
)
set_target_properties(
  sequencer
  PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}"
)

add_executable(
  ${PROJECT_NAME}
  src/main.cpp
)
# set_target_properties(
#   ${PROJECT_NAME}
#   PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}"
# )
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE atomic
  sequencer
  Boost::log
)

enable_testing()

add_executable(
  sequencer_test
  test/sequencer_test.cpp
)
# set_target_properties(
#   sequencer_test
#   PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}"
# )
target_link_libraries(
  sequencer_test
  PRIVATE atomic
  sequencer
  GTest::gtest_main
  Boost::log
)
include(GoogleTest)
gtest_discover_tests(sequencer_test)
