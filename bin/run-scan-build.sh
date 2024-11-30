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
echo "Running scan-build (AKA clang analyzer, subset of clang-tidy)..."

# shellcheck source=/dev/null
source ./.envrc

"${LLVM_HOME}"/bin/scan-build cmake --build ./build
