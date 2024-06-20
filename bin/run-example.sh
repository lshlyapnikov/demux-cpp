#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

msg_num=${1:-10000000}

# start publisher
./build/shm_demux pub 2 "${msg_num}" > ./example-pub.out 2>&1 &
pub_pid="$!"

# let the publisher start and initialize all shared memory objects
sleep 2s

# start subscribers

./build/shm_demux sub 1 "${msg_num}" &> ./example-sub-1.out &
./build/shm_demux sub 2 "${msg_num}" &> ./example-sub-2.out &

# report the state
#ps -ef|grep -F "./build/shm_demux"
pgrep --full --list-full "./build/shm_demux"

# wait for publisher to exist
wait "$pub_pid"

# find the generated XXH64_hash values
grep --color=auto -F "XXH64_hash:" ./example-*.out
