#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"
echo ""
echo "Running clang-format..."

"${LLVM_HOME}"/bin/clang-format --dry-run --Werror ./src/**/*
