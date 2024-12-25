#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"


#changed_files=$( (git diff --cached --name-only --diff-filter=ACMR && git ls-files --others --exclude-standard) | \
#grep -E '\.(cpp|h)$' | \
#sort | uniq )

changed_files=$( (git diff --name-only --diff-filter=ACMR && git ls-files --others --exclude-standard) | \
    grep -E '\.(cpp|h)$' | \
    sort | uniq \
)

for file in ${changed_files}; do
    ./bin/run-clang-tidy.sh "${file}"
done
