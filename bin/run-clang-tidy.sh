#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"
echo ""
echo "Running clang-tidy..."

# if [ -d "${HOME}/.cltcache" ]; then
#     find "${HOME}/.cltcache" -mtime +60 -0 | xargs rm
# fi

clang-tidy-17 -p=./build "$@"
# cltcache clang-tidy-17 -p=./build "$@"
# clang-tidy-17 -p=./build ./src/* ./test/*
# clang-tidy-17 -p=./build ./src/*.cpp ./test/*.cpp
