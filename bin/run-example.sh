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

#valgrind_cmd="valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --track-fds=yes --log-file=valgrind.out"

#CPUPROFILE=shm_demux_pub.prof

# start writer expecting 2 readers
./build/shm_demux pub 2 "${msg_num}" > ./example-pub.out 2>&1 &
pub_pid="$!"

# let the writer start and initialize all shared memory objects, it will wait for both readers
sleep 2s

# start 2 readers

./build/shm_demux sub 1 "${msg_num}" &> ./example-sub-1.out &
./build/shm_demux sub 2 "${msg_num}" &> ./example-sub-2.out &

# report the state
#ps -ef|grep -F "./build/shm_demux"
pgrep --full --list-full "./build/shm_demux"

# wait for publisher to exit
wait "$pub_pid"

# find the generated XXH64_hash values for manual check
grep --color=auto -F "XXH64_hash:" ./example-*.out
