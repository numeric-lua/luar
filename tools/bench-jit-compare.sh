#!/usr/bin/env bash
# Compare the J4 benchmark group with Luar JIT enabled and disabled.
# This is a report, not a hard performance gate.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$ROOT"
mkdir -p tests/tmp
REPEAT=${REPEAT:-3}
WARMUP=${WARMUP:-1}
SCALE=${SCALE:-0.05}
FILTER=${FILTER:-j4}
./src/luar bench/bench.lua "$FILTER" --repeat "$REPEAT" --warmup "$WARMUP" \
  --scale "$SCALE" --json > tests/tmp/bench-jit-on.json
LUAR_BENCH_NOJIT=1 ./src/luar bench/bench.lua "$FILTER" --repeat "$REPEAT" \
  --warmup "$WARMUP" --scale "$SCALE" --json > tests/tmp/bench-jit-off.json
python3 - <<'PY'
import json
on=json.load(open('tests/tmp/bench-jit-on.json'))
off=json.load(open('tests/tmp/bench-jit-off.json'))
def rows(x):
    if isinstance(x, dict):
        for key in ('results','benchmarks','data'):
            if key in x: return x[key]
    return x
a=rows(on); b=rows(off)
am={r['name']:r for r in a if isinstance(r,dict) and 'name' in r}
bm={r['name']:r for r in b if isinstance(r,dict) and 'name' in r}
print(f"{'benchmark':28} {'jit':>10} {'nojit':>10} {'speedup':>8}")
for name in sorted(set(am)&set(bm)):
    x=am[name].get('median_cpu') or am[name].get('mean_cpu')
    y=bm[name].get('median_cpu') or bm[name].get('mean_cpu')
    if x and y:
        print(f"{name:28} {x:10.3f} {y:10.3f} {y/x:8.2f}x")
PY
