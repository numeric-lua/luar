# LuaR benchmark corpus

`bench/` contains a small, self-contained microbenchmark corpus and a
driver that compares LuaR against stock Lua 5.5.1 and LuaJIT 2.1 (JIT on
and off).  It is a measurement tool: there are no pass/fail gates and the
numbers are never committed by default.

| File | Purpose |
| --- | --- |
| `bench.lua` | corpus + timing harness (`--bench`, `--repeat`, `--json`, `--baseline`, `--report`) |
| `compare.sh` | build/snapshot LuaR, run all interpreters, write `tests/tmp/bench-results.md` |
| `README.md` | this file |

Measured numbers and interpretation live in `doc/performance.md`.

## Quick start

```sh
# Full comparison: lua, luar, luajit, luajit -joff; 2 passes, 7 samples each.
./bench/compare.sh

# Reuse the current src/luar instead of running make first.
BUILD=0 ./bench/compare.sh

# One interpreter, quick look.
./src/luar bench/bench.lua all
./src/luar bench/bench.lua numeric --repeat 3
lua           bench/bench.lua all --json > /tmp/lua.json
luajit -joff  bench/bench.lua all --baseline /tmp/lua.json

# Write the report somewhere else (e.g. a reviewed snapshot).
OUT=bench/results.md ./bench/compare.sh
```

The default report is `tests/tmp/bench-results.md`, which is git-ignored.
Do not commit generated numbers: they depend on the host, compiler, build
layout and thermal state.

## What `compare.sh` does

1. Runs `make build` in the repository (unless `BUILD=0`).
2. Copies the resulting `src/luar` to a private temporary path and verifies
   that the copy runs.  Another agent may rebuild `src/luar` concurrently;
   all timings use the snapshot, and its SHA-256 is recorded in the report.
3. Runs the corpus under stock Lua, LuaR, LuaJIT (JIT on) and LuaJIT
   (`-joff`), **one fresh interpreter process per benchmark**.  A fresh
   process matters: a preceding `table/hash` workload changes LuaJIT's GC
   and string-interning state enough to distort a following string
   benchmark by an order of magnitude.
4. Repeats the whole set `PASSES` times, reversing interpreter order on
   even passes so slow thermal drift does not always favour the same
   runtime.  Samples are pooled by `bench.lua --report`.
5. Writes a Markdown report with best/median CPU milliseconds, throughput
   and ratios relative to stock Lua, plus notes for skipped benchmarks.

### Environment variables

| Variable | Default | Meaning |
| --- | --- | --- |
| `LUA_BIN` | `lua` | stock Lua 5.5.1 binary |
| `LUAJIT_BIN` | `luajit` | LuaJIT 2.1 binary |
| `LUAR_BIN` | `src/luar` | LuaR binary to build/reuse |
| `BUILD` | `1` | `1` run `make build`; `0` reuse `LUAR_BIN` |
| `MAKE` | `make` | make command |
| `REPEAT` | `7` | timed samples per benchmark per pass |
| `WARMUP` | `3` | warmup runs before sampling |
| `PASSES` | `2` | full passes; samples are pooled for best/median |
| `SCALE` | `1` | multiply workload sizes |
| `FILTER` | `all` | `all`, a group (`numeric`), a name, or a comma list |
| `OUT` | `tests/tmp/bench-results.md` | Markdown output path |
| `TMPBASE` | `tests/tmp` | scratch parent directory |
| `JITOFF_FLAGS` | `-joff` | flags for the JIT-off run |
| `TIMEOUT` | `180` | seconds per interpreter invocation (`0` disables) |
| `KEEP` | `0` | keep scratch JSON/logs, and print their path |

`LUA_BIN`, `LUAJIT_BIN` and `LUAR_BIN` may be absolute paths or names on
`PATH`.  The script warns if the Lua/LuaJIT versions are not 5.5.1/2.1.

## Corpus

Each benchmark is a plain Lua function in `bench.lua`; the workloads are
written in Lua 5.1/5.5-compatible syntax so the same file runs everywhere.
Sizes are tuned for roughly 20-35 ms per run on a current x86-64
interpreter at `SCALE=1`; use `--scale` on much slower/faster machines.

| Group | Benchmark | Workload |
| --- | --- | --- |
| numeric | `numeric/int_sum` | 8,000,000-iteration integer accumulation |
| numeric | `numeric/float_loop` | 4,000,000-iteration double accumulation |
| numeric | `numeric/fib` | recursive `fib(27)` |
| numeric | `numeric/mandel` | 320×320 Mandelbrot inner loop, 60 iterations max |
| table | `table/array` | 1,400,000-element array fill + read |
| table | `table/hash` | 60,000 pre-built string keys, insert + lookup |
| table | `table/new_clear` | `table.new(500000,0)` + fill + `table.clear` + refill (guarded) |
| string | `string/concat` | 20,000 `s = s .. "x"` concatenations |
| string | `string/format` | 50,000 `string.format("%d:%s:%.3f", ...)` calls |
| string | `string/gsub` | 2,000 `gsub("(%a+)(%d+)", "%2%1")` on a 700-byte string |
| string | `string/pack` | 200,000 `string.pack("<I4", i)` calls (skipped if absent) |
| object | `object/closures` | 1,500,000 calls of a closure over an upvalue |
| object | `object/method` | 1,000,000 `obj:add(i)` method dispatches |
| object | `object/metatable` | 1,000,000 missing-key reads through function `__index` |
| coroutine | `coroutine/ping_pong` | two coroutines yielding alternately, 125,000 rounds each |
| varargs | `varargs/select` | 80,000 calls with 8 varargs and a `select` loop |

Feature notes:

- `table/new_clear` tries `table.new`/`table.clear` (global or
  `require("table.new")`/`require("table.clear")`, which is how LuaJIT
  exposes them).  If unavailable it falls back to `table.create` (Lua 5.5)
  or a plain `{}`, and a `pairs()`-based clear.  The exact path is printed
  in Notes for each interpreter, because this is not an identical
  operation: it measures the best available preallocation/clear facility.
- `string/pack` is unavailable in LuaJIT 2.1 and is reported as `n/a`,
  not as a crash.

## Timing methodology

- **Fresh process per benchmark** (`compare.sh`): no leftover GC heap,
  string table, code cache or JIT traces from another benchmark.
- **Warmup**: 3 untimed runs by default.  This lets LuaJIT compile hot
  traces before sampling.
- **Pilot batching**: after warmup, one pilot run estimates the per-run
  cost and chooses a batch size so a timed sample lasts about 20 ms
  (clamped to 1..200 runs).  The reported time is always per run, so
  interpreters that need larger batches stay comparable.  The chosen
  batch size is visible in `--json` and in the single-run table.
- **Samples**: 7 timed samples per benchmark per pass; two passes are
  pooled by default (14 samples).  Best and median CPU times are reported.
  Best-of-N is the robust lower bound; the median shows the central
  tendency under throttling.
- **CPU time**: `os.clock()` (C `clock()`, CPU time on Linux).  This is
  the primary metric and is insensitive to descheduling.
- **Wall time**: measured around the first timed sample with
  `date +%s%N`; falls back to `/proc/uptime` (10 ms resolution) and then
  `os.clock()`.  The pre-sample GC happens before the wall clock starts,
  so the figure excludes the harness's explicit collection; it still
  includes first-sample effects, normal OS scheduling, and the spawn of
  the closing `date` process (~1-2 ms).  Wall time is reported by
  `bench.lua` and stored in JSON, but the Markdown comparison uses CPU
  time because the host is shared and wall time is not precise here.
- **GC**: `collectgarbage("collect")` before each sample, outside the
  timed region, so a sample does not inherit an arbitrary heap size.
  GC performed by the workload itself is inside the measurement.
- **Guards**: every benchmark run is inside `pcall`.  A crash, a different
  result or a nondeterministic sample marks that benchmark `error` in JSON
  and the report, and the run continues.  Missing features are `skipped`
  (`n/a`).  The harness still exits 0.

`bench.lua` run directly (without `compare.sh`) executes the selected
benchmarks in one process.  That is convenient but not isolated: a
preceding large table/hash benchmark can change the next benchmark's GC
and string state, especially under LuaJIT.  Use `compare.sh` for numbers
you intend to compare across interpreters.

## Reading the output

The single-run table and the Markdown report show:

- **best ms** — fastest CPU time per run among the samples.
- **median ms** — median CPU time per run.
- **runs/s** — `1 / best_cpu`, i.e. how many fixed-size runs per second
  (from the best sample).
- **vs lua / `x`** — `time(this interpreter) / time(stock Lua)`.
  `1.00x` is equal, `0.80x` is 20% faster, `1.20x` is 20% slower.
  Treat ratios close to 1.00 as equal; below ~0.95 or above ~1.05 the
  direction is usually stable, but the magnitude is build- and host-
  dependent.

## Caveats

- **LuaR 0.1 is an interpreter, not a JIT.**  It has no native code
  generation (the `jit` table truthfully reports off) and no FFI in a
  default build.  LuaJIT's JIT-on rows show what a mature JIT achieves;
  they are not LuaR 0.1's behaviour and not a promise for the first LuaR
  JIT tier.
- **The same core does not imply identical timings.**  LuaR's VM, table
  and string sources are byte-identical to vendored Lua 5.5.1, but it is a
  different binary: extra libraries are linked, registration changes the
  heap layout, and the distro-built stock Lua uses its own compiler flags
  and link layout.  `doc/performance.md` includes a control build of the
  vendored sources with the same flags; it shows ±10-20% movement on
  individual microbenchmarks from build layout alone.  Do not attribute
  small differences to language semantics.
- **Thermal and shared-host noise.**  This laptop-class CPU throttles
  10-20%; other agents/builds and desktop processes may run concurrently.
  Best-of-N is optimistic by construction; the median is also reported.
  A full run is a snapshot, not a guarantee.
- **Microbenchmarks.**  They isolate loops, calls, tables and strings.
  They say nothing about startup, I/O, modules, FFI calls or application
  behaviour.
- **Adaptive batching.**  Fast interpreters execute several runs per
  sample.  That measures sustained throughput of repeated identical work
  and lets the JIT settle; it is not the same as first-call latency.
- **Not every workload is comparable.**  `table/new_clear` uses different
  primitives per interpreter (noted), and `string/pack` is absent in
  LuaJIT 2.1 (reported `n/a`).  LuaJIT is Lua 5.1; the corpus avoids
  5.2+/5.5-only syntax.
- **Wall clock is secondary.**  On a throttled, shared host wall time
  includes scheduling and frequency effects; CPU time is the metric used
  for ratios.

## Adding a benchmark

Append a table to `BENCH` in `bench.lua`:

```lua
def {
  name  = "group/name",        -- unique; "group" must be stable
  group = "group",
  skip  = function ()          -- optional: return a reason to mark n/a
    if not some_feature then return "feature unavailable" end
  end,
  prepare = function () ... end, -- optional, untimed, once per process
  note  = "what this measures",  -- optional, printed in Notes
  run   = function ()            -- one fixed-size workload
    local n = N(100000)          -- N() applies --scale
    ...
    return checksum              -- must be deterministic and non-nil
  end,
}
```

The harness runs `run()` once for the expected result, then warmup, then
samples; every result is compared with `==` to catch accidental changes.
Keep `run()` self-contained and free of global state that earlier
benchmarks could alter.  Add the workload to the table above and to
`doc/performance.md` when you publish numbers.

## Make target

The top-level `Makefile` is intentionally not edited by the benchmark
work.  The coordinator will wire:

```make
bench: build
	./src/luar bench/bench.lua all
```

For the full cross-interpreter comparison (and the isolated measurement
used in `doc/performance.md`), use `./bench/compare.sh` directly.
