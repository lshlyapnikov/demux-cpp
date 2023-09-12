#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

###

cd "${__dir}"
./run-cppcheck.sh && ./run-clang-tidy.sh
