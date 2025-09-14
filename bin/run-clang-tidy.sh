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

jobs=$(( $(nproc) > 2 ? $(nproc) - 2 : 1 ))

echo ""
echo "Running clang-tidy, mode: ${mode}, jobs: ${jobs} ..."

all_src_folders=(./src/demux/core/* ./src/demux/util/* ./src/demux/example/* ./src/demux/test/*)

# shellcheck source=/dev/null
source ./.envrc

options=(-quiet -j "${jobs}")

if [ "${mode}" = "all" ]; then
    "${LLVM_HOME}"/bin/run-clang-tidy "${options[@]}" -p=./build "${all_src_folders[@]}"
else
    "${LLVM_HOME}"/bin/run-clang-tidy "${options[@]}" -p=./build "$@"
fi
