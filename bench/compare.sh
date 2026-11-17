#!/usr/bin/env bash
#---------------------------------------------------------------------
# bench/compare.sh -- build (or reuse) LuaR, snapshot the binary, then
# run the bench/bench.lua corpus under:
#
#   stock Lua 5.5.1 (baseline)
#   LuaR
#   LuaJIT 2.1, JIT on
#   LuaJIT 2.1, JIT off (-joff)
#
# and write a Markdown report.  Numbers land under tests/tmp/ by default
# (git-ignored) so benchmark noise is not committed; override OUT to
# publish a snapshot explicitly.
#
# Everything is configurable through the environment:
#
#   LUA_BIN        stock Lua (default: lua on PATH)
#   LUAJIT_BIN     LuaJIT (default: luajit on PATH)
#   LUAR_BIN       LuaR binary (default: <repo>/src/luar)
#   BUILD          1 = run `make build` first, 0 = reuse (default: 1)
#   MAKE           make command (default: make)
#   REPEAT         timed samples per benchmark (default: 7)
#   WARMUP         warmup runs per benchmark (default: 3)
#   PASSES         full passes, pooled by bench.lua --report (default: 2)
#   SCALE          workload scale (default: 1)
#   FILTER         bench selector: all, group, name, comma list (default: all)
#   OUT            Markdown output (default: tests/tmp/bench-results.md)
#   TMPBASE        scratch parent (default: tests/tmp)
#   JITOFF_FLAGS   flags for the JIT-off run (default: -joff)
#   TIMEOUT        per-interpreter timeout in seconds (default: 180, 0 = none)
#   KEEP           1 = keep scratch dir (default: 0)
#
# Examples:
#   ./bench/compare.sh
#   BUILD=0 REPEAT=9 PASSES=3 OUT=bench/results.md ./bench/compare.sh
#   FILTER="numeric,object" ./bench/compare.sh
#---------------------------------------------------------------------
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$ROOT"

LUA_BIN=${LUA_BIN:-lua}
LUAJIT_BIN=${LUAJIT_BIN:-luajit}
LUAR_BIN=${LUAR_BIN:-$ROOT/src/luar}
BUILD=${BUILD:-1}
MAKE=${MAKE:-make}
REPEAT=${REPEAT:-7}
WARMUP=${WARMUP:-3}
PASSES=${PASSES:-2}
SCALE=${SCALE:-1}
FILTER=${FILTER:-all}
OUT=${OUT:-$ROOT/tests/tmp/bench-results.md}
TMPBASE=${TMPBASE:-$ROOT/tests/tmp}
JITOFF_FLAGS=${JITOFF_FLAGS:--joff}
TIMEOUT=${TIMEOUT:-180}
KEEP=${KEEP:-0}

die () { printf 'compare.sh: %s\n' "$*" >&2; exit 1; }
have () { command -v "$1" >/dev/null 2>&1; }

case "$REPEAT" in ''|*[!0-9]*) die "REPEAT must be a positive integer" ;; esac
[ "$REPEAT" -ge 1 ] || die "REPEAT must be a positive integer"
case "$WARMUP" in ''|*[!0-9]*) die "WARMUP must be a non-negative integer" ;; esac
case "$PASSES" in ''|*[!0-9]*) die "PASSES must be a positive integer" ;; esac
[ "$PASSES" -ge 1 ] || die "PASSES must be a positive integer"
case "$TIMEOUT" in ''|*[!0-9]*) die "TIMEOUT must be a non-negative integer" ;; esac
[ "$BUILD" = 0 ] || [ "$BUILD" = 1 ] || die "BUILD must be 0 or 1"
[ "$KEEP" = 0 ] || [ "$KEEP" = 1 ] || die "KEEP must be 0 or 1"

# Resolve binaries that may be bare PATH names or explicit paths.
resolve_bin () {
  local b=$1
  case "$b" in
    */*)
      [ -x "$b" ] || die "not executable: $b"
      printf '%s\n' "$b"
      ;;
    *)
      have "$b" || die "not found on PATH: $b"
      command -v "$b"
      ;;
  esac
}
LUA_BIN=$(resolve_bin "$LUA_BIN")
LUAJIT_BIN=$(resolve_bin "$LUAJIT_BIN")

mkdir -p -- "$TMPBASE"
TMP=$(mktemp -d "$TMPBASE/bench.XXXXXX")
cleanup () {
  if [ "$KEEP" = 1 ]; then
    printf 'scratch kept: %s\n' "$TMP"
  else
    rm -rf -- "$TMP"
  fi
}
trap cleanup EXIT

mkdir -p -- "$(dirname -- "$OUT")"

printf '== LuaR benchmark comparison ==\n'
printf '   repo: %s\n' "$ROOT"
printf '   scratch: %s\n' "$TMP"

# ---------------------------------------------------------------------
# Build and snapshot LuaR before timing.  A concurrent agent may be
# rebuilding src/luar while this runs; the snapshot is the source of
# truth for all timings.
# ---------------------------------------------------------------------
if [ "$BUILD" = 1 ]; then
  printf '== building LuaR: %s -C %s build ==\n' "$MAKE" "$ROOT"
  if ! "$MAKE" -C "$ROOT" build >"$TMP/build.log" 2>&1; then
    tail -n 60 "$TMP/build.log" >&2 || true
    die "build failed (log: $TMP/build.log; rerun with KEEP=1 to keep it)"
  fi
else
  printf '== BUILD=0: reusing %s ==\n' "$LUAR_BIN"
fi

LUAR_COPY="$TMP/luar"
snapshot_luar () {
  local n=0
  while [ "$n" -lt 40 ]; do
    if [ -x "$LUAR_BIN" ] && cp -- "$LUAR_BIN" "$LUAR_COPY" 2>/dev/null; then
      chmod +x -- "$LUAR_COPY" 2>/dev/null || true
      if "$LUAR_COPY" -v >/dev/null 2>&1; then
        return 0
      fi
    fi
    n=$((n + 1))
    sleep 0.25
  done
  return 1
}
snapshot_luar || die "could not snapshot a runnable LuaR binary from $LUAR_BIN"

LUAR_SHA=$(sha256sum -- "$LUAR_COPY" 2>/dev/null | awk '{print $1}')
[ -n "$LUAR_SHA" ] || LUAR_SHA="(sha256sum unavailable)"
SOURCE_SHA=$(sha256sum -- "$LUAR_BIN" 2>/dev/null | awk '{print $1}' || true)
if [ -n "$SOURCE_SHA" ] && [ "$SOURCE_SHA" != "$LUAR_SHA" ]; then
  printf 'warning: %s changed while being snapshotted; timings use the snapshot\n' "$LUAR_BIN" >&2
fi
printf '   LuaR snapshot: %s (%s)\n' "$LUAR_COPY" "$LUAR_SHA"

# ---------------------------------------------------------------------
# Interpreter versions and build flags for the report header.
# ---------------------------------------------------------------------
LUA_VERSION=$("$LUA_BIN" -v 2>&1 | head -n 1)
LUAJIT_VERSION=$("$LUAJIT_BIN" -v 2>&1 | head -n 1)
LUAR_VERSION=$("$LUAR_COPY" -v 2>&1 | head -n 1)

case "$LUA_VERSION" in
  *"Lua 5.5.1"*) ;;
  *) printf 'warning: expected stock Lua 5.5.1, got: %s\n' "$LUA_VERSION" >&2 ;;
esac
case "$LUAJIT_VERSION" in
  *"LuaJIT 2.1"*) ;;
  *) printf 'warning: expected LuaJIT 2.1, got: %s\n' "$LUAJIT_VERSION" >&2 ;;
esac

read -r -a JITOFF_ARGS <<<"$JITOFF_FLAGS"
JIT_ON_STATUS=$("$LUAJIT_BIN" -e 'io.write(tostring(jit.status()))' 2>&1 | head -n 1 || true)
JIT_OFF_STATUS=$("$LUAJIT_BIN" "${JITOFF_ARGS[@]}" -e 'io.write(tostring(jit.status()))' 2>&1 | head -n 1 || true)

if "$MAKE" -s -C "$ROOT/src" echo >"$TMP/flags.txt" 2>/dev/null; then
  BUILD_FLAGS=$(tr '\n' ' ' <"$TMP/flags.txt" | sed 's/  */ /g; s/ $//')
else
  BUILD_FLAGS="(make -C src echo unavailable)"
fi

# ---------------------------------------------------------------------
# Run the corpus.  Pass 1 runs lua -> luar -> luajit -> luajit-joff;
# even passes reverse the order so slow thermal drift does not always
# favour the same interpreter.  bench.lua --report pools the samples.
# ---------------------------------------------------------------------
run_cmd () {
  if [ "$TIMEOUT" -gt 0 ] && have timeout; then
    timeout "$TIMEOUT" "$@"
  else
    "$@"
  fi
}

run_one () {
  local pass=$1 label=$2 out=$3
  shift 3
  local err="$TMP/pass${pass}-${label}.err"
  local -a args
  args=("$@" "$ROOT/bench/bench.lua" "$FILTER"
        --repeat "$REPEAT" --warmup "$WARMUP" --scale "$SCALE"
        --json --label "$label")
  if [ "$label" != "lua" ] && [ -s "$TMP/pass1-lua.json" ]; then
    args+=(--baseline "$TMP/pass1-lua.json")
  fi
  printf '   pass %d: %-12s ' "$pass" "$label"
  if run_cmd "${args[@]}" >"$out" 2>"$err"; then
    printf 'ok\n'
  else
    printf 'FAILED\n'
    sed 's/^/     /' "$err" >&2 || true
    die "benchmark run failed: pass $pass $label (JSON: $out)"
  fi
}

for ((pass = 1; pass <= PASSES; pass++)); do
  if (( pass % 2 == 0 )); then
    ORDER=(luajit-joff luajit luar lua)
  else
    ORDER=(lua luar luajit luajit-joff)
  fi
  for label in "${ORDER[@]}"; do
    case "$label" in
      lua)
        run_one "$pass" lua "$TMP/pass${pass}-lua.json" "$LUA_BIN"
        ;;
      luar)
        run_one "$pass" luar "$TMP/pass${pass}-luar.json" "$LUAR_COPY"
        ;;
      luajit)
        run_one "$pass" luajit "$TMP/pass${pass}-luajit.json" "$LUAJIT_BIN"
        ;;
      luajit-joff)
        run_one "$pass" luajit-joff "$TMP/pass${pass}-luajit-joff.json" \
          "$LUAJIT_BIN" "${JITOFF_ARGS[@]}"
        ;;
    esac
  done
done

# ---------------------------------------------------------------------
# Report: let bench.lua merge the JSON files (pooling samples across
# passes) and emit Markdown tables.
# ---------------------------------------------------------------------
FILES=()
for ((pass = 1; pass <= PASSES; pass++)); do
  for label in lua luar luajit luajit-joff; do
    f="$TMP/pass${pass}-${label}.json"
    [ -s "$f" ] && FILES+=("$f")
  done
done
[ "${#FILES[@]}" -gt 0 ] || die "no result files were produced"

if ! "$LUA_BIN" "$ROOT/bench/bench.lua" --report "${FILES[@]}" >"$TMP/report-body.md" 2>"$TMP/report.err"; then
  sed 's/^/     /' "$TMP/report.err" >&2 || true
  die "failed to build the Markdown report"
fi

CPU_MODEL=$(awk -F': ' '/^model name/ {print $2; exit}' /proc/cpuinfo 2>/dev/null | sed 's/[[:space:]]*$//' || true)
NCPU=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo '?')
GCC_LINE=$(gcc --version 2>/dev/null | head -n 1 || true)

{
  printf '# LuaR benchmark comparison\n\n'
  printf -- '- Generated: `%s`\n' "$(date -Iseconds 2>/dev/null || date)"
  printf -- '- Host: `%s`\n' "$(uname -a)"
  printf -- '- CPU: %s (%s logical CPUs)\n' "${CPU_MODEL:-unknown}" "$NCPU"
  [ -n "$GCC_LINE" ] && printf -- '- Compiler: `%s`\n' "$GCC_LINE"
  printf -- '- Build flags: `%s`\n' "$BUILD_FLAGS"
  printf -- '- LuaR: `%s`, snapshot sha256 `%s`\n' "$LUAR_VERSION" "$LUAR_SHA"
  printf -- '- Stock Lua: `%s` at `%s`\n' "$LUA_VERSION" "$LUA_BIN"
  printf -- '- LuaJIT: `%s` at `%s`\n' "$LUAJIT_VERSION" "$LUAJIT_BIN"
  printf -- '- JIT status: on-run `%s`, off-run (`%s`) `%s`\n' \
    "${JIT_ON_STATUS:-?}" "$JITOFF_FLAGS" "${JIT_OFF_STATUS:-?}"
  printf -- '- Corpus: `%s`; workload scale `%s`\n' "$FILTER" "$SCALE"
  printf -- '- Method: warmup %s, %s timed CPU samples per pass, %s pass(es) pooled; ' \
    "$WARMUP" "$REPEAT" "$PASSES"
  printf 'pilot batching targets ~20 ms per sample; GC collected between samples.\n'
  printf -- '- Command: `./bench/compare.sh` (REPEAT=%s WARMUP=%s PASSES=%s SCALE=%s)\n\n' \
    "$REPEAT" "$WARMUP" "$PASSES" "$SCALE"
  cat "$TMP/report-body.md"
  printf '\n## Caveats\n\n'
  printf -- '- These are microbenchmarks, not application benchmarks. Ratios are specific to this host, build and run.\n'
  printf -- '- LuaR 0.1 is interpreter-only: no native JIT, no FFI. The LuaJIT rows are the performance ceiling a JIT can reach, not LuaR 0.1 behaviour.\n'
  printf -- '- The machine is shared and throttles; best-of-N is optimistic, the median is reported alongside it. Runs with ratios near 1.00 should be treated as equal.\n'
  printf -- '- Stock Lua lacks `table.new`/`table.clear`; `table/new_clear` uses a `table.create` + `pairs()` fallback and says so in Notes. LuaJIT 2.1 lacks `string.pack`, so that benchmark is `n/a` there.\n'
  printf -- '- Wall time includes explicit GC between samples and harness overhead; CPU time is the primary metric.\n'
  printf -- '- Numeric results are intentionally not committed: default output is under `tests/tmp/`. Regenerate with `./bench/compare.sh`.\n'
} >"$OUT"

printf '== report written to %s ==\n' "$OUT"
if [ "$KEEP" = 1 ]; then
  printf '== raw JSON + logs: %s ==\n' "$TMP"
fi
