#!/usr/bin/env bash
set -euo pipefail

BIN="./optimize_threshold.out"
OUT_DIR="test_result"

# N values to test (edit as needed)
Ns=(10000 20000 50000 100000 200000 500000 1000000 2000000 5000000)

mkdir -p "$OUT_DIR"

for N in "${Ns[@]}"; do
  out_csv="${OUT_DIR}/results_N${N}.csv"
  echo "Running N=$N -> $out_csv" >&2
  "$BIN" "$N" 15 0.25 0.95 0.05 1 > "$out_csv"
done

echo "Done. CSV files are in: $OUT_DIR" >&2