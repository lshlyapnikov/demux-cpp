#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
__root="$(cd "$(dirname "${__dir}")" && pwd)"

###

cd "${__root}"

msg_num=${1:-10000000}
zero_copy=${2:-"false"}
calculate_hash=${3:-"true"}

report_state_and_generate_kill_command() {
    if [ -z "$1" ]; then
        echo "Usage: report_state_and_generate_kill_command <search_pattern>" >&2
        return 1
    fi

    pgrep --full --list-full "$1"
 
    local pids
    pids=$(pgrep --full "$1" | tr '\n' ' ' | sed 's/ *$//')

    if [ -z "$pids" ]; then
        echo "No processes found matching: $1" >&2
        return 0
    fi

    echo "# to kill, use this command: kill $pids"
}

# start writer expecting 2 readers
#CPUPROFILE=shm_demux_writer.prof CPUPROFILE_FREQUENCY=1000 \
# perf record -F 999 --call-graph fp \
./build/shm_demux writer 0,1 "${msg_num}" "${zero_copy}" "${calculate_hash}" > ./example-writer.log 2>&1 &
writer_pid="$!"

# let the writer start and initialize all shared memory objects, it will wait for both readers
sleep 2s

# start 2 readers

./build/shm_demux reader 0 "${msg_num}" "${zero_copy}" "${calculate_hash}" &> ./example-reader-0.log &
reader_0_pid="$!"
./build/shm_demux reader 1 "${msg_num}" "${zero_copy}" "${calculate_hash}" &> ./example-reader-1.log &
reader_1_pid="$!"

# report the state
#ps -ef|grep -F "./build/shm_demux"
# pgrep --full --list-full "./build/shm_demux"
report_state_and_generate_kill_command "./build/shm_demux"

# wait for the writer AND both readers to exit, otherwise the readers may still be
# flushing their logs when we grep for the hash codes below
wait "$writer_pid" "$reader_0_pid" "$reader_1_pid"

# find the generated XXH64_hash values for manual check
grep --color=auto -F "XXH64_hash:" ./example-*.log

hash_codes=()

for log_file in ./example-*.log; do
    hash=$(grep -F "XXH64_hash: " "$log_file" | awk -F 'XXH64_hash: ' '{print $2}' | awk '{print $1}') || true
    if [[ -z "$hash" ]]; then
        echo "No XXH64_hash found in ${log_file}" >&2
        continue
    fi
    hash_codes+=("$hash")
done

if [[ "${#hash_codes[@]}" -ne 3 ]]; then
  echo "Expected 3 hash codes (1 writer + 2 readers), got ${#hash_codes[@]}: " "${hash_codes[@]+"${hash_codes[@]}"}" >&2
  exit 101
fi

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
