#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
#__file="${__dir}/$(basename "${BASH_SOURCE[0]}")"
#__base="$(basename ${__file} .sh)"
__root="$(cd "$(dirname "${__dir}")" && pwd)" # <-- change this as it depends on your app

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