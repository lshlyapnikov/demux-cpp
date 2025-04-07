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
zero_copy=${2:-"false"}

#valgrind_cmd="valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --track-fds=yes --log-file=valgrind.out"

# start writer expecting 2 readers
#CPUPROFILE=shm_demux_writer.prof CPUPROFILE_FREQUENCY=1000 \
./build/shm_demux writer 2 "${msg_num}" "${zero_copy}" > ./example-writer.out 2>&1 &
writer_pid="$!"

# let the writer start and initialize all shared memory objects, it will wait for both readers
sleep 2s

# start 2 readers

./build/shm_demux reader 1 "${msg_num}" "${zero_copy}" &> ./example-reader-1.out &
./build/shm_demux reader 2 "${msg_num}" "${zero_copy}" &> ./example-reader-2.out &

# report the state
#ps -ef|grep -F "./build/shm_demux"
pgrep --full --list-full "./build/shm_demux"

# wait for writer to exit
wait "$writer_pid"

# find the generated XXH64_hash values for manual check
grep --color=auto -F "XXH64_hash:" ./example-*.out

hash_codes=()

for log_file in ./example-*.out; do
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
  echo "Found unequals hash codes: " "${hash_codes[@]}"
  exit 100
fi

# generate profiler report
#google-pprof --text ./build/shm_demux ./shm_demux_writer.prof &> pprof-report.out
