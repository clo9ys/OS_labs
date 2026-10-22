#!/bin/bash
set -euo pipefail

BIN=${BIN:-./build/minmax}
SIZES=${SIZES:-"10000 100000 1000000 10000000 100000000"}
THREADS=${THREADS:-"1 2 3 4 5 6 8 10 12 16"}

echo "n,threads,avg_ms,best_ms"
for n in $SIZES; do
    r=$((1000000000 / n))
    if [ "$r" -gt 300 ]; then r=300; fi
    if [ "$r" -lt 20 ]; then r=20; fi
    for t in $THREADS; do
        "$BIN" -t "$t" -n "$n" -r "$r" -s 42 -q
    done
done
