#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

# -C, --clear   Clear the entire cache, removing all cached files, but keeping the configuration file.
ccache --clear

rm -rf ./*.log
rm -rf ./*.prof
rm -rf ./perf.data*
rm -rf ./core*
