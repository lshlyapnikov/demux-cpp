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
echo "Running clang-tidy-diff..."

git diff -U0 HEAD^ | /usr/bin/clang-tidy-diff-17.py \
  -clang-tidy-binary /usr/bin/clang-tidy-17 \
  -path ./build -p1 -use-color
