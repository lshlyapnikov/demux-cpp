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
use_emplace=${2:-"false"}
calculate_hash=${3:-"true"}

# start writer expecting 2 readers
# CPUPROFILE=example-writer.prof CPUPROFILE_FREQUENCY=1000 \
# perf stat \
taskset -c 3 \
./build/shm_demux writer 2 "${msg_num}" "${use_emplace}" "${calculate_hash}" > ./example-writer.log 2>&1 &
writer_pid="$!"

# let the writer start and initialize all shared memory objects, it will wait for both readers
sleep 2s

# start 2 readers

# CPUPROFILE=example-reader-0.prof CPUPROFILE_FREQUENCY=1000 \
# perf record -e task-clock,context-switches,cpu-migrations,page-faults,instructions,cycles,branches,branch-misses \
# perf record -e branches,branch-misses \
# perf stat \
# perf record -e L1-dcache-load-misses,L1-icache-load-misses,LLC-load-misses \
# perf stat -e instructions,cycles,branches,branch-misses,cache-references,cache-misses,L1-dcache-load-misses \
# perf record -e L1-dcache-load-misses:u -g \
# perf stat \
taskset -c 4 \
./build/shm_demux reader 0 "${msg_num}" "${use_emplace}" "${calculate_hash}" &> ./example-reader-0.log &

#CPUPROFILE=example-reader-1.prof CPUPROFILE_FREQUENCY=1000 \
# perf stat \
taskset -c 5 \
./build/shm_demux reader 1 "${msg_num}" "${use_emplace}" "${calculate_hash}" &> ./example-reader-1.log &

# report the state
#ps -ef|grep -F "./build/shm_demux"
pgrep --full --list-full "./build/shm_demux"

# wait for writer to exit
wait "$writer_pid"

# find the generated XXH64_hash values for manual check
grep --color=auto -F "XXH64_hash:" ./example-*.log

hash_codes=()

for log_file in ./example-*.log; do
    hash=$(grep -F "XXH64_hash: " "$log_file" | awk -F 'XXH64_hash: ' '{print $2}' | awk '{print $1}')
    hash_codes+=("$hash")
done

all_equal=true
first_element="${hash_codes[0]}"

for item in "${hash_codes[@]}"; do
  if [[ "$item" != "$first_element" ]]; then
    all_equal=false
    break
  fi
done

if $all_equal; then
  echo "All Write and Read hash codes are equal: " "${hash_codes[@]}"
else
  echo "Found unequal hash codes: " "${hash_codes[@]}"
  exit 100
fi

# generate profiler report
#google-pprof --text ./build/shm_demux ./shm_demux_writer.prof &> pprof-report.log
