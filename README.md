# C++ Lock-free Demultiplexer Queue

**A Lock-free, Thread-safe, Shared Memory and CPU Cache Optimized Demultiplexer Queue** designed for high-performance concurrent programming. This queue supports a single producer (publisher) and multiple consumers (subscribers), ensuring efficient, contiguous data distribution without locking mechanisms. The queue features zero-copy on the consumer side and is optimized for shared memory environments, making it ideal for applications requiring fast, non-blocking communication between threads or processes.

_Description generated with the assistance of ChatGPT._

## 1. Shared Memory Demultiplexer Queue Latency (ns)

Test conducted on my laptop without CPU isolation or CPU pinning. Refer to the **Run Example** section for more details.

```
       Value   Percentile   TotalCount 1/(1-Percentile)

        79.0     0.000000           50         1.00
       103.0     0.250000      3394829         1.33
       119.0     0.500000      5070828         2.00
       127.0     0.625000      6357200         2.67
       143.0     0.750000      7827810         4.00
       151.0     0.812500      8849381         5.33
       151.0     0.875000      8849381         8.00
       159.0     0.906250      9257164        10.67
     32767.0     0.999999      9999993   1398101.33
     38911.0     1.000000     10000000   2796202.67
     38911.0     1.000000     10000000          inf
#[Mean    =      128.848, StdDeviation   =      276.737]
#[Max     =    38911.000, Total count    =     10000000]
#[Buckets =           28, SubBuckets     =           32]
```

## 2. Development Environment

This project was developed and tested exclusively on Linux using Clang version 17.0.6.

**Prerequisites:**

- **CMake 3.16.3**
- **Boost 1.83.0:** Configured in the [CMakeList.txt](./CMakeLists.txt)
- **Clang 17.0.6:** Configured in the [.envrc](./.envrc)
- **gperftools** (optional): google-perftools package
- **direnv** (optional): An environment variable manager for your shell. More information can be found [here](https://direnv.net/)

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

Execute tests using the following command:

```
$ cmake --build ./build --target test
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
valgrind_cmd="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.out"
${valgrind_cmd} ./build/shm_demux pub 2 "${msg_num}" > ./example-pub.out 2>&1 &
```

or

```
valgrind_cmd="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.out"
${valgrind_cmd} ./build/shm_demux sub 1 "${msg_num}" &> ./example-sub-1.out &
```

### 10.2. Run the example script

```
$ ./bin/run-example.sh
```

**Notes:** `--track-origins=yes` can be slow. The report will be generated in the `valgrind.out` file.

## 11. Performance Profiling with gperftools (Google Performance Tools)

https://github.com/gperftools/gperftools/tree/master

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
CPUPROFILE=shm_demux_pub.prof ./build/shm_demux pub 2 "${msg_num}" > ./example-pub.out 2>&1 &
```

### 11.3. Run the example script

```
$ ./bin/run-example.sh
```

### 11.4. Generate the profiling report

```
$ google-pprof --text ./build/shm_demux ./shm_demux_pub.prof &> pprof-report.out
```

## 12. Links

- [LLVM Debian/Ubuntu nightly packages](https://apt.llvm.org/)
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
