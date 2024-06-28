cmake_minimum_required(VERSION 3.14)
project(demux-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_VERBOSE_MAKEFILE ON) # verbose CMake output

# standard CMake build types: Debug, Release, RelWithDebInfo, MinSizeRel
message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

IF(CMAKE_BUILD_TYPE MATCHES Debug)
  message("Debug build.")
ELSEIF(CMAKE_BUILD_TYPE MATCHES Release)
  message("Release build.")
ELSE()
  message("Defaulting to Release build.")
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()

#message("CMAKE_ENABLE_PROFILING=${CMAKE_ENABLE_PROFILING}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc--clang
#set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wconversion -Wsign-conversion -Werror")
set(MY_CXX_FLAGS -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wconversion -Wsign-conversion -Werror)

# valgrind needs -gdwarf-4
set(CMAKE_CXX_FLAGS_DEBUG "-gdwarf-4 -O0")
#set(CMAKE_CXX_FLAGS_DEBUG "-gdwarf-4 -O3 -DNDEBUG") # -03 for gperftools
# set(CMAKE_CXX_FLAGS_RELEASE "-O3")
# set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
# set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os")

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

# example uses this to compute hash digest of sent and received messages
FetchContent_Declare(
  xxhash
  SYSTEM
  GIT_REPOSITORY git@github.com:Cyan4973/xxHash.git
  GIT_TAG 805c00b68fa754200ada0c207ffeaa7a4409377c
)
FetchContent_MakeAvailable(xxhash)

# example uses this to measure and report message latency
FetchContent_Declare(
  hdr_histogram
  SYSTEM
  GIT_REPOSITORY git@github.com:HdrHistogram/HdrHistogram_c.git
  GIT_TAG e33c19c911bd259a84dcb2b86c2e4bd5b79258e1
)
FetchContent_MakeAvailable(hdr_histogram)

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
  SYSTEM ${xxhash_SOURCE_DIR}
  SYSTEM ${hdr_histogram_SOURCE_DIR}/include
  SYSTEM ${rapidcheck_SOURCE_DIR}/include
)

#
# artifacts
#

add_executable(
  shm_demux
  src/example/shm_demux.cpp
  src/example/market_data.cpp
)
target_link_libraries(
  shm_demux
  domain
  atomic
  hdr_histogram
  rt
  Boost::log
  #profiler
)
target_compile_options(
  shm_demux
  PRIVATE ${MY_CXX_FLAGS}
)

add_library(
  domain
  OBJECT src/main/domain.cpp
)
target_compile_options(
  domain
  PRIVATE ${MY_CXX_FLAGS}
)

add_library(
  demultiplexer
  OBJECT src/main/demultiplexer.cpp
)
target_compile_options(
  demultiplexer
  PRIVATE ${MY_CXX_FLAGS}
)

enable_testing()
include(GoogleTest)

add_executable(
  domain_test
  src/test/domain_test.cpp
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
  message_buffer_test
  src/test/message_buffer_test.cpp
)
target_link_libraries(
  message_buffer_test
  GTest::gtest_main
  rapidcheck
  Boost::log
)
target_compile_options(
  message_buffer_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(message_buffer_test)

add_executable(
  demultiplexer_test
  src/test/demultiplexer_test.cpp
)
target_link_libraries(
  demultiplexer_test
  demultiplexer
  domain
  atomic
  GTest::gtest_main
  rapidcheck
  Boost::log
)
target_compile_options(
  demultiplexer_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(demultiplexer_test)
