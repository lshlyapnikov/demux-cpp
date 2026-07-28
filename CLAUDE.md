# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Project

A lock-free, thread-safe, shared-memory, CPU-cache-optimized **demultiplexer queue**:
single producer (writer), multiple consumers (readers), zero-copy on the consumer side.
Header-only core; C++20; Clang-only; Linux-only.

## Environment

Environment variables live in [.envrc](.envrc) — `source ./.envrc` (or `direnv allow .`) before
building or running tooling. Key vars: `CPP_STANDARD=20`, Clang toolchain via `LLVM_HOME`
(`CC`/`CXX`), `RC_PARAMS` (rapidcheck), `CTEST_OUTPUT_ON_FAILURE=1`.

Dependencies are managed with **Conan 2** (profiles in [etc/conan2/profiles/](etc/conan2/profiles/)):
Boost (log, stacktrace), GTest, rapidcheck, xxHash, hdr_histogram.

## Common commands

Run scripts from the repo root; they `cd` to root and source `.envrc` as needed.

- Configure + resolve deps + generate build (Debug default): `./bin/init-cmake-build.sh [Debug|Release|RelWithDebInfo]`
- Build: `cmake --build ./build`
- Release build (clean + configure + build): `./bin/release.sh`
- Run all tests: `cmake --build ./build --target test`
- Run one test by name: `ctest --test-dir ./build -R <NamePattern> -V`
- Run the example (writer + 2 readers, checks hashes): `./bin/run-example.sh`
- Clean build artifacts: `cmake --build ./build --target clean`
- Clean caches/logs: `./bin/clean.sh`

## Linting / formatting

Config: [.clang-format](.clang-format), [.clang-tidy](.clang-tidy). clang-tidy needs
`compile_commands.json` (CMake generates it into `./build`).

- Format check: `./bin/run-clang-format.sh check` — fix in place: `./bin/run-clang-format.sh fix`
- clang-tidy (all): `./bin/run-clang-tidy.sh` — diff only: `./bin/run-clang-tidy-diff.sh`
- Static analyzer: `scan-build-<ver> cmake --build ./build` (see [bin/run-scan-build.sh](bin/run-scan-build.sh))

Warnings are errors: `-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wconversion -Wsign-conversion -Werror`.

## Layout

- [src/demux/core/](src/demux/core/) — queue internals: `demux_writer.h`, `demux_reader.h`,
  `message_buffer.h`, `reader_id.*`, and the WIP `demux_writer_loop.h` / `demux_reader_loop.h`.
- [src/demux/util/](src/demux/util/) — header-only helpers (`result.h`, `fast_math.h`, atomics, shm, logging, etc.).
- [src/demux/example/](src/demux/example/) — `shm_demux` executable (market-data demo, latency histogram).
- [src/demux/test/](src/demux/test/) — GTest + rapidcheck (property-based) tests, one `*_test.cpp` per unit.
- [doc/adr/](doc/adr/) — architecture decision records. [doc/TODO.md](doc/TODO.md) — open work.

Each test is a separate CMake executable registered via `gtest_discover_tests`. Adding a test
means adding an `add_executable` + `target_link_libraries` + `gtest_discover_tests` block in
[CMakeLists.txt](CMakeLists.txt) (a commented template is at the bottom of that file).

## Conventions

- Namespaces: `lshl::demux::{core,util,example}`; tests in `lshl::demux::core::test`.
- Every source file starts with the copyright + `SPDX-License-Identifier: Apache-2.0` header;
  headers use `#pragma once`.
- Prefer `using` declarations for specific symbols over `using namespace`.
- Error handling uses `lshl::demux::util::Result<E, R>` (see [src/demux/util/result.h](src/demux/util/result.h)) —
  a `std::variant`-based Ok/Err type; `EmptyResult`, `empty_value`, `value()`, `error()` helpers.
- Core classes are size-parameterized templates (e.g. `DemuxWriter<L, M, B>`) with `static_assert`
  invariants; the buffer size `L` must be a power of two. Cache-line alignment / false-sharing
  avoidance is deliberate — preserve alignment and padding when editing core types.
