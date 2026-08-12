#!/usr/bin/env bash
set -uo pipefail

mysql_bin=${MYSQL_BIN:-mysql}
server=${MO_TPCH_SERVER:-127.0.0.1}
port=${MO_TPCH_PORT:-6001}
user=${MO_TPCH_USER:-root}
password=${MO_TPCH_PASSWORD:-111}
query_dir=${1:-/root/src/matrixone/test/distributed/cases/benchmark/tpch/03_QUERIES}
output_dir=${2:-/tmp/matrixone-odbc-tpch}

mkdir -p "${output_dir}"
summary="${output_dir}/summary.tsv"
printf 'query\tstatus\telapsed_ms\tresult_lines\n' >"${summary}"

failures=0
for query_number in $(seq 1 22); do
  query="q${query_number}.sql"
  output="${output_dir}/q${query_number}.out"
  error="${output_dir}/q${query_number}.err"
  started=$(date +%s%N)

  MYSQL_PWD="${password}" "${mysql_bin}" \
    --protocol=TCP \
    --host="${server}" \
    --port="${port}" \
    --user="${user}" \
    --ssl-mode=DISABLED \
    --default-character-set=utf8mb4 \
    --batch --raw \
    <"${query_dir}/${query}" >"${output}" 2>"${error}"
  status=$?

  finished=$(date +%s%N)
  elapsed_ms=$(((finished - started) / 1000000))
  result_lines=$(wc -l <"${output}")
  if [[ ${status} -eq 0 ]]; then
    result=PASS
  else
    result=FAIL
    failures=$((failures + 1))
  fi

  printf '%s\t%s\t%s\t%s\n' \
    "${query}" "${result}" "${elapsed_ms}" "${result_lines}" | tee -a "${summary}"
done

if [[ ${failures} -ne 0 ]]; then
  printf '%s TPC-H queries failed. See %s.\n' "${failures}" "${output_dir}" >&2
  exit 1
fi

printf 'All 22 TPC-H queries passed. Summary: %s\n' "${summary}"
