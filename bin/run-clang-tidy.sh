#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

mode=${1:-all}

echo ""
echo "Running clang-tidy, mode: ${mode} ..."

all_src_folders=(./src/demux/* ./src/example/* ./src/test/*)

if [ "${mode}" = "all" ]; then
    "${LLVM_HOME}"/bin/clang-tidy -p=./build "${all_src_folders[@]}"
else
    "${LLVM_HOME}"/bin/clang-tidy -p=./build "$@"
fi
