#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

#msg_num=10000000
msg_num=100

./build/shm_demux pub 2 ${msg_num} > ./example-pub.out 2>&1 &

# let the publisher start and initialize all shared memory objects
sleep 2s

./build/shm_demux sub 1 ${msg_num} &> ./example-sub-1.out &

./build/shm_demux sub 2 ${msg_num} &> ./example-sub-2.out &

ps -ef|grep -F "./build/shm_demux"
