# C++ Lock-free Demultiplexer Queue

**A Lock-free, Thread-safe, Shared Memory Friendly Demultiplexer Queue** designed for high-performance concurrent programming. This queue supports a single producer (publisher) and multiple consumers (subscribers), ensuring efficient data distribution without the need for locking mechanisms. The queue features zero-copy on the consumer side and is optimized for shared memory environments, making it ideal for applications requiring fast, non-blocking communication between threads or processes.

_Description generated with the assistance of ChatGPT._

## Shared Memory Demultiplexer Queue Latency (ns)

Test conducted on my laptop without CPU isolation or CPU pinning. Refer to the **Run Example** section for more details.

```
       Value   Percentile   TotalCount 1/(1-Percentile)

       167.0     0.000000            5         1.00
       215.0     0.250000      3022092         1.33
       239.0     0.500000      5339846         2.00
       271.0     0.750000      8128375         4.00
       271.0     0.812500      8128375         5.33
       303.0     0.906250      9243185        10.67
     98303.0     0.999999     10000000   1048576.00
     98303.0     1.000000     10000000          inf
#[Mean    =      473.791, StdDeviation   =     2617.481]
#[Max     =    98303.000, Total count    =     10000000]
#[Buckets =           28, SubBuckets     =           32]
```

## Development Environment

This project was developed and tested exclusively on Linux using Clang version 17.0.6.

**Prerequisites:**

- **CMake 3.16.3**
- **Boost 1.83.0:** Configured in the [CMakeList.txt](./CMakeLists.txt)
- **Clang 17.0.6:** Configured in the [.envrc](./.envrc)
- **direnv** (optional): An environment variable manager for your shell. More information can be found [here](https://direnv.net/)

## Setting Project Environment Variables

### Using `direnv`

```
$ direnv allow .
```

### Without `direnv`

```
$ source ./.envrc
```

## Generate Project Makefile with CMake

To generate the Makefile for your project using CMake:

```
$ ./bin/init-cmake-build.sh
```

## Build (Debug)

To build the project in debug mode:

```
$ cmake --build ./build
```

## Build (Release)

To build the project in release mode:

```
$ cmake -DCMAKE_BUILD_TYPE=Release --build ./build
```

## Run Tests

Execute tests using the following command:

```
$ cmake --build ./build --target test
```

## Run Example

Explore a usage example of the Shared Memory Demultiplexer Queue: [./example/shm_demux.cpp](./example/shm_demux.cpp)

To run the example:

```
$ ./bin/run-example.sh
```

## Clean Build Artifacts

To clean build artifacts:

```
$ cmake --build ./build --target clean
```

## Profiling with Valgrind

For memory profiling with Valgrind, start a publisher under valgrind:

```
$ valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.out ./build/shm_demux pub 1 1000
```

Start a subscriber:

```
$ ./build/shm_demux sub 1 1000
```

**Notes:** `--track-origins=yes` can be slow.

You can also uncomment `valgrind_cmd` definition in `./bin/run-example.sh` and place it in front of the publisher or subscriber command:

```
valgrind_cmd="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.out"

${valgrind_cmd} ./build/shm_demux pub 2 "${msg_num}" > ./example-pub.out 2>&1 &
```

or

```
valgrind_cmd="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.out"

${valgrind_cmd} ./build/shm_demux sub 1 "${msg_num}" &> ./example-sub-1.out &
```

## Links

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
- [FlatBuffers](https://flatbuffers.dev/flatbuffers_guide_use_cpp.html)
- [MoldUDP64 Protocol Specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf)
