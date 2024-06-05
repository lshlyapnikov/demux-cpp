#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

count=500
for i in $(seq $count); do
    echo "Attempt $i ---------"
    rm -rf ./multiplexer_test.out
    #./build/multiplexer_test > ./multiplexer_test.out
    ./build/multiplexer_test > /dev/null
done
