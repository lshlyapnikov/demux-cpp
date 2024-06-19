# C++ Lock-free Demultiplexer Queue

Lock-free (non-blocking), Shared Memory, Thread-safe Demultiplexer Queue. Single input (publisher), multiple outputs (subscribers).

## Building and Testing

### Generate/Regenerate Project Makefile

```
./bin/init-cmake-build.sh
```

### Debug Build

```
$ cmake --build ./build
```

### Release Build

```
$ cmake -DCMAKE_BUILD_TYPE=Release --build ./build
```

### Run Example

[Shared Memory Demultiplexer Queue Example](./example/shm_demux.cpp)

```
$ ./bin/run-example.sh
```

### Run Tests

```
$ cmake --build ./build --target test
```

### Clean

```
$ cmake --build ./build --target clean
```

### Profile with Valgrind

```
$ valgrind --leak-check=full --show-leak-kinds=all ./build/demultiplexer_test
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
  - [Boost.Program_options](https://www.boost.org/doc/libs/1_83_0/doc/html/program_options.html)
  - [Boost.Interprocess](https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess.html)
    - [Sharing memory between processes](https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess/sharedmemorybetweenprocesses.html)
    - [Managed Memory Segments](https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess/managed_memory_segments.html)
- [MoldUDP64 Protocol Specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf)
- [FlatBuffers](https://flatbuffers.dev/flatbuffers_guide_use_cpp.html)
