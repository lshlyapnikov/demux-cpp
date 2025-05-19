# C++ Lock-free Demultiplexer Queue

**A Lock-free, Thread-safe, Shared Memory and CPU Cache Optimized Demultiplexer Queue** designed for high-performance concurrent programming. This queue supports a single producer (writer) and multiple consumers (readers), ensuring efficient, contiguous data distribution without locking mechanisms. The queue features zero-copy on the consumer side and is optimized for shared memory environments, making it ideal for applications requiring fast, non-blocking communication between threads or processes.

_Description generated with the assistance of ChatGPT._

## 1. Shared Memory Demultiplexer Queue Latency (ns)

Test conducted on my laptop without CPU isolation or CPU pinning. Refer to the **Run Example** section for more details.

```
       Value   Percentile   TotalCount 1/(1-Percentile)

        71.0     0.000000          584         1.00
        95.0     0.250000      2839845         1.33
       111.0     0.500000      5051419         2.00
       119.0     0.625000      6336535         2.67
       135.0     0.750000      7695856         4.00
       143.0     0.812500      8442301         5.33
       151.0     0.875000      8759366         8.00
       167.0     0.906250      9322774        10.67
       479.0     0.996094      9967488       256.00
     77823.0     0.999992     10000000    131072.00
     77823.0     1.000000     10000000          inf
#[Mean    =      130.915, StdDeviation   =      630.619]
#[Max     =    77823.000, Total count    =     10000000]
#[Buckets =           28, SubBuckets     =           32]
```

## 2. Development Environment

This project was developed and tested exclusively on Linux using Clang version 17.0.6 and 18.

### 2.1. Prerequisites

- **Conan 2.7.0** `pip install conan`
- **CMake**
- **Clang 18** Configured in the [.envrc](./.envrc). See <https://apt.llvm.org/>.
- **libstdc++** with C++20 support, `sudo add-apt-repository ppa:ubuntu-toolchain-r/test`
- **gperftools** (optional): google-perftools package.
- **direnv** (optional): An environment variable manager for your shell. More information can be found [here](https://direnv.net/).
- **ccache** (optional): Compiler cache.

### 2.2. Additional Information

For detailed configuration and build workflows, see the project's CI pipeline: [.github/workflows/cmake-single-platform.yml](.github/workflows/cmake-single-platform.yml).

## 3. Setting Project Environment Variables

### 3.1. Using `direnv`

```
$ direnv allow .
```

### 3.2. Without `direnv`

```
$ source ./.envrc
```

## 4. Generate Project Makefile with CMake

To generate the Makefile for your project using CMake:

```
$ ./bin/init-cmake-build.sh
```

## 5. Build (Debug)

To build the project in debug mode:

```
$ cmake --build ./build
```

To build with the Clang Static Analyzer (recommended):

```
$ scan-build-17 cmake --build ./build
```

## 6. Build (Release)

To build the project in release mode:

```
$ ./bin/release.sh
```

## 7. Run Tests

### 7.1. Run All Tests

Execute all tests using cmake:

```
$ cmake --build ./build --target test
```

### 7.2. Run One Test

Execute "SlowReader" test using ctest:

```
$ ctest --test-dir ./build -R SlowReader -V
```

Execute "SlowReader" test using main function:

```
$ ./build/demultiplexer_test --gtest_filter=*SlowReader
```

## 8. Run Example

Explore a usage example of the Shared Memory Demultiplexer Queue: [./example/shm_demux.cpp](./example/shm_demux.cpp)

To run the example:

```
$ ./bin/run-example.sh
```

## 9. Clean Build Artifacts

To clean build artifacts:

```
$ cmake --build ./build --target clean
```

## 10. Memory Profiling with Valgrind

### 10.1. modify `run-example.sh`

```
valgrind_cmd="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.log"
${valgrind_cmd} ./build/shm_demux pub 2 "${msg_num}" > ./example-pub.log 2>&1 &
```

or

```
valgrind_cmd="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.log"
${valgrind_cmd} ./build/shm_demux sub 1 "${msg_num}" &> ./example-sub-1.log &
```

### 10.2. Run the example script

```
$ ./bin/run-example.sh
```

**Notes:** `--track-origins=yes` can be slow. The report will be generated in the `valgrind.log` file.

## 11. Performance Profiling with gperftools (Google Performance Tools)

<https://github.com/gperftools/gperftools/tree/master>

### 11.1. Update `CMakeLists.txt`

To enable optimization in the Debug build, replace `-O0` with `-O3` in the `CMAKE_CXX_FLAGS_DEBUG` definition. This will allow the Debug build to compile with optimization:

```
set(CMAKE_CXX_FLAGS_DEBUG "-gdwarf-4 -O3")
```

Add `profiler` to the `target_link_libraries(shm_demux ...)` section to include `-lprofiler` linking option:

```
target_link_libraries(
  shm_demux
...
  profiler
)
```

### 11.2. Modify `run-example.sh`

Define `CPUPROFILE` to generate a profile file named `shm_demux_pub.prof`:

```
CPUPROFILE=shm_demux_pub.prof ./build/shm_demux pub 2 "${msg_num}" > ./example-pub.log 2>&1 &
```

### 11.3. Run the example script

```
$ ./bin/run-example.sh
```

### 11.4. Generate the profiling report

```
$ google-pprof --text ./build/shm_demux ./shm_demux_writer.prof &> pprof-report.log
```

## 12. Conan Commands

### 12.1. Deploy this package to the local conan repository

```
$ conan create . -s build_type=Debug
```

### 12.2. Remove this package from the local conan repository

```
conan remove --confirm demux-cpp/*
```

## 13. Links

- [LLVM Debian/Ubuntu nightly packages](https://apt.llvm.org/)
- Conan C/C++ Package Manager
  - [Conan Central Repository](https://conan.io/center)
  - [Consuming Conan Packages Tutorial](https://docs.conan.io/2/tutorial/consuming_packages.html)
- [C++ Best Practices](https://github.com/cpp-best-practices/cppbestpractices/blob/master/00-Table_of_Contents.md)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Clang-Tidy](https://clang.llvm.org/extra/clang-tidy/)
- [GoogleTest Primer](https://google.github.io/googletest/primer.html)
- [RapidCheck](https://github.com/emil-e/rapidcheck)
  - [User Guide](https://github.com/emil-e/rapidcheck/blob/master/doc/user_guide.md)
  - [Arbitrary Generators](https://github.com/emil-e/rapidcheck/blob/master/doc/generators.md#arbitrary)
  - [Configuration](https://github.com/emil-e/rapidcheck/blob/master/doc/configuration.md)
- [Boost Library Documentation](https://www.boost.org/doc/libs/)
  - [Boost.Interprocess](https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess.html)
    - [Sharing memory between processes](https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess/sharedmemorybetweenprocesses.html)
    - [Managed Memory Segments](https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess/managed_memory_segments.html)
  - [Boost.Program_options](https://www.boost.org/doc/libs/1_83_0/doc/html/program_options.html)
- [xxHash - Extremely fast hash algorithm](https://github.com/Cyan4973/xxHash)
- [HdrHistogram](http://hdrhistogram.org/)
- [Valgrind](https://valgrind.org/)
- [gperftools (originally Google Performance Tools)](https://github.com/gperftools/gperftools)
- [Latency Numbers Every Programmer Should Know](https://gist.github.com/jboner/2841832)
- [FlatBuffers](https://flatbuffers.dev/flatbuffers_guide_use_cpp.html)
- [MoldUDP64 Protocol Specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf)
