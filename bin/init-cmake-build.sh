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

# import environment variables
# shellcheck source=/dev/null
source ./.envrc

# Conan: configure profile and resolve package dependencies
conan install . --profile:all=./etc/conan2/profiles/clang.profile \
    --build=missing \
    --settings=build_type="${build_type}"

# CMake: generate build files
cd ./build
cmake ../ -DCMAKE_TOOLCHAIN_FILE=./"${build_type}"/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE="${build_type}"
cd ../

echo ""
echo "Use: "
echo "cmake --build ./build"
echo ""
