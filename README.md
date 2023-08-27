# shm-sequencer

Shared Memory Sequencer.

## Building and Testing

### Generate/Regenerate Project Makefile

```
$ ./bin/init-cmake-build.sh
```

### Build

```
$ cmake --build ./build
```

### Run Tests

```
cmake --build ./build --target test
```

## Links

- [Clang-Tidy](https://clang.llvm.org/extra/clang-tidy/)
- [FlatBuffers](https://flatbuffers.dev/flatbuffers_guide_use_cpp.html)
- [GoogleTest Primer](https://google.github.io/googletest/primer.html)
- [Boost.Interprocess](https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess.html)
- [Boost.Interprocess/Managed Memory Segments](https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess/managed_memory_segments.html)
- [MoldUDP64 Protocol Specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf)
