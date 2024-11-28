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

if [ "${build_type}" != "Debug" ] && [ "${build_type}" != "Release" ]; then
    echo "Unsupported build_type: ${build_type} ..."
    echo "Try Release or Debug"
    exit 101
fi

[ -d ./build ] && rm -rf ./build
mkdir ./build

echo "build_type: ${build_type}"

# resolve package dependencies
conan install . --profile:all=./conan2/profiles/clang --output-folder=./build/ \
    --build=missing --settings=build_type="${build_type}"

# generate build files
cd ./build
cmake ../ -DCMAKE_TOOLCHAIN_FILE=./conan_toolchain.cmake -DCMAKE_BUILD_TYPE="${build_type}"
cd ../

echo ""
echo "Use: "
echo "cmake --build ./build"
echo ""
