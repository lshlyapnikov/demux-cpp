cmake_minimum_required(VERSION 3.14)
project(shm-sequencer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_VERBOSE_MAKEFILE ON) # verbose CMake output

add_definitions(-DLOG_LEVEL=debug)

message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

IF(CMAKE_BUILD_TYPE MATCHES Debug)
  message("Debug build.")
ELSEIF(CMAKE_BUILD_TYPE MATCHES Release)
  message("Release build.")
ELSE()
  message("Defaulting to Release build.")
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc--clang
#set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wconversion -Wsign-conversion -Werror")
set(MY_CXX_FLAGS -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wconversion -Wsign-conversion -Werror)

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
  NAMES "clang-tidy-17" REQUIRED
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

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
  googletest
  SYSTEM
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  rapidcheck
  SYSTEM
  GIT_REPOSITORY git@github.com:emil-e/rapidcheck.git
  GIT_TAG ff6af6fc683159deb51c543b065eba14dfcf329b
)
FetchContent_MakeAvailable(rapidcheck)

# system include directories do not raise compiler warnings
include_directories(
  SYSTEM ${Boost_INCLUDE_DIR}
  SYSTEM ${rapidcheck_SOURCE_DIR}/include
)

#
# artifacts
#

add_executable(
  ${PROJECT_NAME}
  src/main.cpp
)
target_link_libraries(
  ${PROJECT_NAME}
  sequencer
  atomic
  Boost::log
)
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE ${MY_CXX_FLAGS}
)

add_library(
  domain
  OBJECT src/domain.cpp
)
target_compile_options(
  domain
  PRIVATE ${MY_CXX_FLAGS}
)

add_library(
  sequencer
  OBJECT src/sequencer.cpp
)
target_compile_options(
  sequencer
  PRIVATE ${MY_CXX_FLAGS}
)

add_library(
  multiplexer
  OBJECT src/multiplexer.cpp
)
target_compile_options(
  multiplexer
  PRIVATE ${MY_CXX_FLAGS}
)

enable_testing()
include(GoogleTest)

add_executable(
  domain_test
  test/domain_test.cpp
)
target_link_libraries(
  domain_test
  domain
  GTest::gtest_main
  rapidcheck
)
target_compile_options(
  domain_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(domain_test)

add_executable(
  sequencer_test
  test/sequencer_test.cpp
)
target_link_libraries(
  sequencer_test
  sequencer
  multiplexer
  domain
  atomic
  GTest::gtest_main
)
target_compile_options(
  sequencer_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(sequencer_test)

add_executable(
  multiplexer_test
  test/multiplexer_test.cpp
)
target_link_libraries(
  multiplexer_test
  multiplexer
  sequencer
  domain
  atomic
  GTest::gtest_main
  rapidcheck
  Boost::log
)
target_compile_options(
  multiplexer_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(multiplexer_test)