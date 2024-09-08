#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

mode=${1:-check}

options="--dry-run"

if [ "${mode}" = "check" ]; then
    # ok it is a dry run by defualt
    echo "Dry run."
elif [ "${mode}" = "fix" ]; then
    echo "Fix formatting inplace."
    options="-i"
else
    echo "Invalid argument"
    echo "Usage: ${0} [check|fix]"
    exit 1
fi

cd "${__root}"
echo "Running clang-format..."

"${LLVM_HOME}"/bin/clang-format "${options}" --Werror ./src/**/*
