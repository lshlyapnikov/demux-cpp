cmake_minimum_required(VERSION 3.15)
project(demux-cpp LANGUAGES CXX)

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

set(LLVM_HOME $ENV{LLVM_HOME})
message("LLVM_HOME=${LLVM_HOME}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF) # Disable GNU extensions
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # generate a compile_commands.json file, needed for clang-tidy

# https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc--clang
#set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wconversion -Wsign-conversion -Werror")
set(MY_CXX_FLAGS -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wconversion -Wsign-conversion -Werror)

set(CMAKE_CXX_FLAGS_DEBUG "-gdwarf-4 -O0") # valgrind needs `-gdwarf-4`
#set(CMAKE_CXX_FLAGS_DEBUG "-g -gdwarf-4 -O3 -DNDEBUG") # `-03 -DNDEBUG` for gperftools

# default settins for other build types:
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

#
# dependencies
#

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS log)

# example uses this to compute hash digest of sent and received messages
find_package(xxHash REQUIRED)

# example uses this to measure and report message latency
find_package(hdr_histogram REQUIRED)

find_package(GTest REQUIRED)

find_package(rapidcheck REQUIRED)

# system include directories do not raise compiler warnings
include_directories(
  SYSTEM ${Boost_INCLUDE_DIR}
  SYSTEM ${xxHash_INCLUDE_DIR}
  SYSTEM ${hdr_histogram_INCLUDE_DIR}
  # SYSTEM ${rapidcheck_INCLUDE_DIR}
)

#
# artifacts
#

add_library(
  ${PROJECT_NAME}
  STATIC
)
target_link_libraries(
  ${PROJECT_NAME}
  reader_id
  demultiplexer
)

add_executable(
  shm_demux
  src/demux/example/shm_demux.cpp
  src/demux/example/market_data.cpp
)
target_link_libraries(
  shm_demux
  reader_id
  hdr_histogram::hdr_histogram_static
  Boost::log
  profiler
)
target_compile_options(
  shm_demux
  PRIVATE ${MY_CXX_FLAGS}
)

add_library(
  reader_id
  OBJECT src/demux/core/reader_id.cpp
)
target_compile_options(
  reader_id
  PRIVATE ${MY_CXX_FLAGS}
)

add_library(
  demultiplexer
  OBJECT src/demux/core/demultiplexer.cpp
)
target_compile_options(
  demultiplexer
  PRIVATE ${MY_CXX_FLAGS}
)

enable_testing()
include(GoogleTest)

add_executable(
  reader_id_test
  src/demux/test/reader_id_test.cpp
)
target_link_libraries(
  reader_id_test
  reader_id
  gtest::gtest
  rapidcheck::rapidcheck
)
target_compile_options(
  reader_id_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(reader_id_test)

add_executable(
  message_buffer_test
  src/demux/test/message_buffer_test.cpp
)
target_link_libraries(
  message_buffer_test
  gtest::gtest
  rapidcheck::rapidcheck
  Boost::log
)
target_compile_options(
  message_buffer_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(message_buffer_test)

add_executable(
  demultiplexer_test
  src/demux/test/demultiplexer_test.cpp
)
target_link_libraries(
  demultiplexer_test
  demultiplexer
  reader_id
  gtest::gtest
  rapidcheck::rapidcheck
  Boost::log
  atomic
)
target_compile_options(
  demultiplexer_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(demultiplexer_test)

add_executable(
  demultiplexer_allocate_test
  src/demux/test/demultiplexer_allocate_test.cpp
)
target_link_libraries(
  demultiplexer_allocate_test
  demultiplexer
  reader_id
  gtest::gtest
  rapidcheck::rapidcheck
  Boost::log
  atomic
)
target_compile_options(
  demultiplexer_allocate_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(demultiplexer_allocate_test)

add_executable(
  shm_util_test
  src/demux/test/shm_util_test.cpp
)
target_link_libraries(
  shm_util_test
  reader_id
  gtest::gtest
  rapidcheck::rapidcheck
  Boost::log
  atomic
)
target_compile_options(
  shm_util_test
  PRIVATE ${MY_CXX_FLAGS}
)
gtest_discover_tests(shm_util_test)
