#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

[ -d ./build ] && rm -rf ./build
mkdir ./build

cd ./build
cmake ../
cd ../

echo ""
echo "Use: "
echo "cmake --build ./build"
echo ""