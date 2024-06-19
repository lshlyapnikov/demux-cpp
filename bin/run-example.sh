#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

msg_num=10000000

./build/shm_demux pub 2 ${msg_num} > ./pub.out 2>&1 &

sleep 2s

./build/shm_demux sub 1 ${msg_num} &> ./sub-1.out &

./build/shm_demux sub 2 ${msg_num} &> ./sub-2.out &

ps -ef|grep -F "./build/shm_demux"
