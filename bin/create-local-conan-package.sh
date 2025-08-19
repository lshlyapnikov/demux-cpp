#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

# Debug/Release
build_type="${1:-Debug}"

echo "build_type: ${build_type}"

conan remove --confirm demux-cpp/*
conan create . -s build_type="${build_type}" --profile:all=./etc/conan2/profiles/clang-"${build_type}"
