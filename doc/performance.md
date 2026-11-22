# Luar performance baseline

> **0.155 refresh (2026-11-22, `./bench/compare.sh`, Ryzen 3 7320U).**
> Best CPU milliseconds; `x` relative to stock Lua 5.5.1.
>
> | benchmark | Lua 5.5 | Luar 0.155 | Luar x | LuaJIT 2.1 | LuaJIT x |
> | --- | ---: | ---: | ---: | ---: | ---: |
> | numeric/int_sum | 35.35 | **4.02** | 0.11 | 5.95 | 0.17 |
> | numeric/float_sum | 8.49 | **1.53** | 0.18 | 1.48 | 0.17 |
> | numeric/product | 17.89 | **2.99** | 0.17 | 2.94 | 0.16 |
> | numeric/fib | 13.43 | 15.34 | 1.14 | 1.40 | 0.10 |
> | table/array | 34.93 | **3.81** | 0.11 | 12.96 | 0.37 |
> | table/hash | 11.96 | 5.31 | 0.44 | 3.58 | 0.30 |
> | j4/ffi_faddi_call | n/a | **20.78** | - | n/a | - |
> | j4/ffi_ccall3dd | n/a | **31.21** | - | n/a | - |
> | j4/ffi_ccall2dd | n/a | **32.66** | - | n/a | - |
> | j4/ffi_ccall2ii | n/a | 139.03 | - | n/a | - |
>
> Integer/table-array loops remain far ahead of stock Lua; FADDI-backed
> native calls run near the plain double-call forms.

> **0.129 refresh (2026-11-22, `./bench/compare.sh`, Ryzen 3 7320U).**
> Best CPU milliseconds; `x` relative to stock Lua 5.5.1.
>
> | benchmark | Lua 5.5 | Luar 0.129 | Luar x | LuaJIT 2.1 | LuaJIT x |
> | --- | ---: | ---: | ---: | ---: | ---: |
> | numeric/int_sum | 35.48 | **3.99** | 0.11 | 5.99 | 0.17 |
> | numeric/float_sum | 8.52 | **1.52** | 0.18 | 1.48 | 0.17 |
> | numeric/product | 17.62 | **2.96** | 0.17 | 2.95 | 0.17 |
> | numeric/fib | 13.42 | 15.42 | 1.15 | 1.40 | 0.10 |
> | table/array | 33.97 | **3.80** | 0.11 | 12.00 | 0.35 |
> | table/hash | 9.19 | 5.17 | 0.56 | 3.56 | 0.39 |
> | string/concat | n/a in this run | - | - | - | - |
>
> `j4/ffi_faddi_call` (FADDI argument to a native `fabs` call) records
> 24.93 ms and `j4/ffi_ccall2ff` (`hypotf(3.0, 4.0)`) 26.08 ms in the
> 0.158 benchmark run.
>
> These remain microbenchmarks.  Luar wins the integer/table-array loops,
> ties `product`, and loses recursive `fib` and hash insertion.


> **J4 additions (0.109–0.110).**  The corpus includes `j4/float_addi`
> (`s = s + (v + 1)` through `FADDI`) and `j4/long_loop` (a 220-statement
> loop body).  On the development host `make bench-jit` measured
> **1.50x** for `float_addi` and **4.13x** for `long_loop`, JIT-on versus
> JIT-off.  These are microbenchmarks, not wall-clock claims.


> **0.98 snapshot (2026-11-21, `./bench/compare.sh`, Ryzen 3 7320U).**
> Best CPU milliseconds; `x` is relative to stock Lua 5.5.1.  This is a
> microbenchmark corpus, not an application claim.
>
> | benchmark | Lua 5.5 | Luar 0.98 | Luar x | LuaJIT 2.1 | LuaJIT x |
> | --- | ---: | ---: | ---: | ---: | ---: |
> | numeric/int_sum | 37.83 | **4.27** | 0.11 | 6.06 | 0.16 |
> | numeric/float_loop | 27.78 | 6.97 | 0.25 | 3.35 | 0.12 |
> | numeric/product | 18.07 | 3.12 | 0.17 | 3.14 | 0.17 |
> | numeric/float_sum | 9.09 | **1.55** | 0.17 | 1.50 | 0.17 |
> | numeric/fib | 13.99 | 15.22 | 1.09 | 1.37 | 0.10 |
> | numeric/mandel | 21.89 | 20.55 | 0.94 | 1.49 | 0.07 |
> | table/array | 35.33 | **3.82** | 0.11 | 12.86 | 0.36 |
> | table/hash | 11.36 | 7.67 | 0.68 | 3.88 | 0.34 |
> | string/concat | 7.09 | 6.73 | 0.95 | 14.99 | 2.11 |
> | object/closures | 44.49 | **0.38** | 0.01 | 1.16 | 0.03 |
>
> Luar wins integer/float-sum/table-array loops against LuaJIT on this host,
> ties `product`, and loses recursive `fib`/`mandel`, float-loop and hash
> cases.  Full rows and caveats are regenerated with `./bench/compare.sh`;
> benchmark output stays under `tests/tmp/`.


> The measured tables below are the historical 0.1 interpreter baseline.
> Current releases add the default J2/J3 loop JIT and the opt-in J4 tracing
> IR; regenerate current numbers with `./bench/compare.sh`, including the
> Luar-only `j4` corpus group.

> **J2 update (2026-11-17, later the same day).**  The x86-64 loop JIT
> compiles hot `for` loops whose bodies are numeric register arithmetic
> (MOVE/LOADI/LOADF/LOADK/ADDI/ADD/SUB/MUL/DIV and the K variants), mixing
> int and double operands with guards.  In the same harness,
> `numeric/int_sum` dropped from ~35 ms to **3.9 ms** (0.11x stock Lua,
> faster than LuaJIT's 5.9 ms), and `numeric/float_loop` from ~27 ms to
> **6.8 ms** (0.25x stock; LuaJIT is 3.0 ms) after register-cached
> `acc + i`, `acc + i * const`, `acc + i * i` and simple `while` fast
> paths.  `fib` is not compiled (calls); `mandel` remains interpreter-bound
> and is near parity after the `-O3` default.  The tables below are the
> pre-J2 baseline and remain valid for everything the JIT does not compile.
> See the J4 snapshot below for current numbers.  The experimental table JIT is
> opt-in and does not yet beat the interpreter on this corpus.  Re-run
> `make bench-compare` for current numbers.

> **J4 snapshot (2026-11-18, after making `-O3` the default build level;
> `make bench-compare`).**
> Best CPU milliseconds; `x` is relative to stock Lua (lower is faster).
>
> | benchmark | lua | luar | luar x | luajit | luajit x |
> | --- | ---: | ---: | ---: | ---: | ---: |
> | numeric/int_sum | 35.26 | **3.91** | 0.11 | 5.87 | 0.17 |
> | numeric/float_loop | 27.43 | **6.85** | 0.25 | 3.16 | 0.12 |
> | numeric/float_div | 10.28 | **6.36** | 0.62 | 6.36 | 0.62 |
> | numeric/float_sum | 8.33 | **1.52** | 0.18 | 1.47 | 0.18 |
> | numeric/product | 17.71 | **2.94** | 0.17 | 2.94 | 0.17 |
> | numeric/lin_sum | 53.01 | **3.92** | 0.07 | 5.97 | 0.11 |
> | numeric/square_sum | 56.97 | **3.92** | 0.07 | 5.93 | 0.10 |
> | numeric/repeat_sum | 47.65 | **1.96** | 0.04 | 7.83 | 0.16 |
> | numeric/while_sum | 59.88 | **1.96** | 0.03 | 7.83 | 0.13 |
> | numeric/fib | 13.39 | 13.61 | 1.02 | 1.47 | 0.11 |
> | numeric/mandel | 20.90 | 21.22 | 1.02 | 1.42 | 0.07 |
> | table/array | 24.94 | **4.70** | 0.19 | 8.60 | 0.34 |
> | table/hash | 8.94 | **3.63** | 0.41 | 3.51 | 0.39 |
> | table/new_clear | 16.34 | **10.28** | 0.63 | 2.16 | 0.13 |
>
> The register-cached fast paths are the source of the integer, float,
> product, linear, square, repeat, while, float division and float sums;
> `table/new_clear` is
> faster through the Luar extension/interpreter path.  `fib` is unchanged because calls are not
> compiled; `mandel` is now at parity.  Default raw table fill/sum intrinsics
> make `table/array` about 0.18x stock Lua and `table/hash` about 0.44x.
> Simple upvalue-accumulator calls, one-parameter leaf arithmetic calls,
> one-field `self:add` methods and `__index` multiplier loops are inlined.
> `object/closures`, `object/method`, `object/metatable`,
> `object/leaf_call`, `object/leaf_chain`, `object/leaf_twoarg`,
> `object/leaf_threearg`, `object/leaf_branch`,
> `object/leaf_twoarg_branch` and `object/leaf_fourarg` are all faster than
> stock Lua; the first four also beat LuaJIT.  Generic table JIT remains
> opt-in; arbitrary method dispatch and general calls still interpret.  Preallocating the fill array from the loop's
> remaining count was also tested; it did not make the native fill faster than
> the interpreter, so it was not kept.  `-O3` (the default from this build)
> improved the interpreter path without changing semantics.  The official suite, ASan and the whole test
> matrix pass at this revision.  `make JIT=0` now genuinely rebuilds the
> interpreter-only binary (previously the `JIT` alias was ignored).

Measured on 2026-11-17 (America/Sao_Paulo) against stock Lua 5.5.1 and
LuaJIT 2.1, on the machine described below.  These are the pre-J2 baseline
numbers (interpreter, no FFI); they remain the honest starting point that
the tiered JIT improves on, and the J2 note above records the loops that
are now compiled.

## Conditions

| Item | Value |
| --- | --- |
| Host | `Linux machinedr 7.3.0-rc6-1-MANJARO #1 SMP PREEMPT_DYNAMIC Mon, 05 Oct 2026 00:15:36 +0000 x86_64 GNU/Linux` |
| CPU | AMD Ryzen 3 7320U with Radeon Graphics, 8 logical CPUs (4 cores) |
| Compiler | gcc (GCC) 16.2.1 20260810 |
| Luar build | `make` (`make -C src`), `CC=gcc`, `CFLAGS=-O2 -g -Wall -Wextra -DLUA_USE_LINUX` in this pre-J2 baseline; the default is now `-O3`, `LIBS=-lm -Wl,-E -ldl`, `WITH_FFI=0` |
| Luar binary | `Luar 0.1.0 (Lua 5.5.1)`, sha256 `508f45817e158d3c6a04e939d209e2e50ced9cfe15f71a25ef821f0f23162588` |
| Stock Lua | `/usr/bin/lua`, package `lua 5.5.1-1`, `Lua 5.5.1`, sha256 `3eab886a2dcd0f09362ed8fbd8f843b51bf0946270a87e48c46f9edb2e0d8dcb`; `.comment` says GCC 16.1.1 |
| LuaJIT | `/usr/bin/luajit`, package `luajit 2.1.1788856981+c6ffc14-1`, `LuaJIT 2.1.1788856981`, sha256 `573d15af0b8e3c040cb42b2dcba8d77c29d5097e200e27cf3fddbda0def89230` |
| Revision | HEAD `270abe4fc5cf6b218b208511812f20210bf54277`; the build also contained unrelated in-progress FFI edits, not linked with `WITH_FFI=0` |
| Date of run | 2026-11-17 07:59:13 -03:00 |

Core provenance: `lvm.c`, `ltable.c`, `lstrlib.c`, `lstring.c`,
`lobject.c`, `lgc.c`, `lapi.c`, `ldo.c` and `lbaselib.c` are byte-identical
to `vendor/lua-5.5.1/src/` at this revision (zero diff lines).  Only the
CLI (`lua.c`) differs.  Luar's runtime differences are additive
(`luar_lib.c`, `luar_jit.c`), so the VM behaviour measured
here is upstream Lua 5.5.1 behaviour.

The machine is shared and laptop-class: another agent was building in the
same checkout during the measurement, desktop processes were active, and
the CPU throttles roughly 10-20%.  The load average was roughly 2-3 on 8
logical CPUs while the runs were being made.

## Method

```sh
# Build and snapshot (compare.sh copies src/luar before timing).
make build

# 4 passes for stability; compare.sh defaults to 2.
REPEAT=7 WARMUP=3 PASSES=4 \
  OUT=tests/tmp/bench-results-4pass.md \
  ./bench/compare.sh
```

The measurements above used a private snapshot of that build (sha256
`508f45817e158d3c6a04e939d209e2e50ced9cfe15f71a25ef821f0f23162588`);
`BUILD=0 LUAR_BIN=<that binary>` reproduces the exact interpreter.

`bench/compare.sh` runs every benchmark in a **fresh interpreter process**
for each of stock Lua 5.5.1, Luar, LuaJIT 2.1 with the JIT on, and LuaJIT
2.1 with `-joff`.  For each benchmark and interpreter:

1. run once for the expected result, then 3 warmup runs;
2. run a pilot to choose a batch so one timed sample lasts about 20 ms
   (1..200 runs; time is always divided by the batch);
3. collect 7 timed CPU samples (`os.clock()`, i.e. CPU time), with
   `collectgarbage("collect")` before each sample and outside the timer;
4. report the best and the median of all pooled samples.  Four passes
   were run, reversing interpreter order on even passes, giving 28
   samples per benchmark and interpreter.  `x` factors are ratios of best
   CPU times; the table also shows medians.

Wall time is sampled once per benchmark around the first timed sample
with `date +%s%N` (fallbacks: `/proc/uptime`, then `os.clock()`).  It is
a sanity check only; the ratios use CPU time because the host is shared.
Wall clock ran about 1.05-1.6x the CPU figure depending on allocation and
GC activity; allocation-heavy `table/hash` was the worst case at roughly
1.9-2.1x.  The raw JSON contains both.

No pass/fail gate exists.  Missing features are reported as `n/a`
(`string.pack` under LuaJIT 2.1) and broken runs would be reported as
`error`; none occurred in this run.

## Results

Best (median) CPU milliseconds per fixed-size run, best-of-28 samples.
`x` = time relative to stock Lua; lower is faster.  `n/a` means the
interpreter does not provide the feature.

| benchmark | lua | luar | luar x | luajit | luajit x | luajit -joff | -joff x |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| numeric/int_sum | 37.30 (38.92) | 32.30 (34.29) | 0.87 | 6.01 (6.33) | 0.16 | 25.49 (27.44) | 0.68 |
| numeric/float_loop | 28.26 (29.90) | 26.84 (28.75) | 0.95 | 3.18 (3.32) | 0.11 | 16.73 (17.69) | 0.59 |
| numeric/fib | 13.65 (13.90) | 13.90 (14.57) | 1.02 | 1.39 (1.53) | 0.10 | 8.91 (9.45) | 0.65 |
| numeric/mandel | 21.78 (22.65) | 22.66 (23.86) | 1.04 | 1.42 (1.48) | 0.07 | 11.25 (11.50) | 0.52 |
| table/array | 26.07 (28.60) | 26.39 (28.67) | 1.01 | 9.30 (10.71) | 0.36 | 20.02 (21.81) | 0.77 |
| table/hash | 11.47 (18.22) | 11.47 (18.49) | 1.00 | 3.75 (4.39) | 0.33 | 4.36 (4.92) | 0.38 |
| table/new_clear | 16.82 (17.91) | 12.97 (15.43) | 0.77 | 2.39 (2.78) | 0.14 | 5.88 (6.09) | 0.35 |
| string/concat | 7.00 (7.49) | 7.14 (7.71) | 1.02 | 15.57 (16.14) | 2.22 | 18.41 (19.87) | 2.63 |
| string/format | 29.18 (31.09) | 31.63 (33.79) | 1.08 | 5.87 (6.49) | 0.20 | 9.81 (10.35) | 0.34 |
| string/gsub | 24.67 (26.03) | 27.62 (29.16) | 1.12 | 24.79 (26.31) | 1.00 | 24.23 (25.70) | 0.98 |
| string/pack | 25.37 (26.91) | 27.95 (29.56) | 1.10 | n/a | n/a | n/a | n/a |
| object/closures | 33.13 (34.32) | 32.63 (34.33) | 0.98 | 1.16 (1.19) | 0.03 | 21.64 (22.69) | 0.65 |
| object/method | 33.37 (35.76) | 32.60 (34.96) | 0.98 | 0.80 (0.82) | 0.02 | 23.62 (24.70) | 0.71 |
| object/metatable | 31.72 (33.56) | 36.79 (38.11) | 1.16 | 0.99 (1.02) | 0.03 | 23.97 (25.06) | 0.76 |
| coroutine/ping_pong | 29.27 (30.12) | 30.66 (32.02) | 1.05 | 8.68 (9.42) | 0.30 | 8.36 (8.88) | 0.29 |
| varargs/select | 29.37 (30.56) | 33.22 (34.93) | 1.13 | 1.74 (1.83) | 0.06 | 14.41 (14.65) | 0.49 |

Throughput, from the best CPU sample (runs/second):

| benchmark | lua | luar | luajit | luajit -joff |
| --- | ---: | ---: | ---: | ---: |
| numeric/int_sum | 26.8 | 31.0 | 166.4 | 39.2 |
| numeric/float_loop | 35.4 | 37.3 | 314.6 | 59.8 |
| numeric/fib | 73.3 | 71.9 | 717.5 | 112.2 |
| numeric/mandel | 45.9 | 44.1 | 703.6 | 88.9 |
| table/array | 38.4 | 37.9 | 107.5 | 50.0 |
| table/hash | 87.2 | 87.2 | 267.0 | 229.6 |
| table/new_clear | 59.4 | 77.1 | 417.7 | 170.0 |
| string/concat | 142.8 | 140.0 | 64.2 | 54.3 |
| string/format | 34.3 | 31.6 | 170.3 | 101.9 |
| string/gsub | 40.5 | 36.2 | 40.3 | 41.3 |
| string/pack | 39.4 | 35.8 | n/a | n/a |
| object/closures | 30.2 | 30.6 | 865.4 | 46.2 |
| object/method | 30.0 | 30.7 | 1260 | 42.3 |
| object/metatable | 31.5 | 27.2 | 1010 | 41.7 |
| coroutine/ping_pong | 34.2 | 32.6 | 115.2 | 119.6 |
| varargs/select | 34.0 | 30.1 | 576.1 | 69.4 |

Geometric means of best-CPU time ratios (lower/faster than Lua is good):

| comparison | geomean | speedup | benches |
| --- | ---: | ---: | ---: |
| Luar / Lua 5.5.1 | 1.013 | 0.99x (1.3% slower) | 16 |
| LuaJIT -joff / Lua 5.5.1 | 0.609 | 1.64x faster | 15 |
| LuaJIT JIT-on / Lua 5.5.1 | 0.149 | 6.69x faster | 15 (no `string.pack`) |

By group (speedup over stock Lua, same data):

| group | Luar | LuaJIT -joff | LuaJIT JIT-on |
| --- | ---: | ---: | ---: |
| numeric | 1.03x | 1.65x | 9.54x |
| table | 1.09x | 2.14x | 3.92x |
| string | 0.93x | 1.05x | 1.31x |
| object | 0.96x | 1.42x | 33.74x |
| coroutine | 0.95x | 3.50x | 3.37x |
| varargs | 0.88x | 2.04x | 16.92x |

## Interpretation

### Luar vs stock Lua 5.5.1

They are the same VM, and the aggregate result says so: Luar is 1.3%
slower on the best-CPU geometric mean over the 16 benchmarks, i.e. parity
within the noise of this host.  Individual rows still move by up to 16%,
in both directions, which is expected for two different binaries rather than two
different implementations.

- **Effectively equal (within about 2%):** `numeric/fib` (1.02),
  `table/array` (1.01), `table/hash` (1.00), `string/concat` (1.02),
  `object/closures` (0.98), `object/method` (0.98).
- **Luar slower:** `object/metatable` 1.16x (16%),
  `varargs/select` 1.13x, `string/gsub` 1.12x, `string/pack` 1.10x,
  `string/format` 1.08x, `coroutine/ping_pong` 1.05x, `numeric/mandel`
  1.04x.  These are real for this pair of binaries (see the repeatability
  note below), but they are not caused by semantic changes.
- **Luar faster:** `table/new_clear` 0.77x (23% faster, because Luar
  provides `table.new`/`table.clear` and stock Lua falls back to
  `table.create` plus a Lua-level `pairs()` clear), `numeric/int_sum`
  0.87x, `numeric/float_loop` 0.95x.  The integer/float wins are build
  artefacts, not a Luar optimization: a control build of pristine vendor
  Lua with the same compiler and flags also runs `int_sum` at 0.92x stock
  Lua.
- **What is not different:** the language, VM, table, string and GC
  sources are upstream.  Luar 0.1 cannot be expected to outperform Lua
  5.5 on pure interpreter workloads; it only wins where an extension
  removes work.

The differences are dominated by build layout and compiler version, not
by Luar's additive libraries.  A control build was made from
`vendor/lua-5.5.1/` with the same `-O2 -g -Wall -Wextra` and GCC 16.2.1,
then compared with stock Lua (GCC 16.1.1) and Luar over 5 rounds of 9
samples per benchmark.  Medians of the per-round best times:

| benchmark | stock Lua | vendor, same flags | Luar | Luar / same-flags ref |
| --- | ---: | ---: | ---: | ---: |
| numeric/int_sum | 35.81 | 32.96 (0.92x) | 31.86 (0.89x) | 0.97 |
| numeric/mandel | 21.54 | 22.17 (1.03x) | 22.42 (1.04x) | 1.01 |
| table/hash | 13.28 | 12.79 (0.96x) | 12.55 (0.95x) | 0.98 |
| string/gsub | 23.82 | 28.32 (1.19x) | 27.18 (1.14x) | 0.96 |
| string/pack | 25.17 | 26.97 (1.07x) | 27.53 (1.09x) | 1.02 |
| object/metatable | 31.05 | 34.70 (1.12x) | 37.55 (1.21x) | 1.08 |

The same-flags reference moves in the same direction as Luar for most
workloads (int_sum faster, gsub/metatable/pack slower), even though it
contains no Luar code at all.  Luar stays within 8% of that reference and
within 2-4% on most rows.  The remaining spread is code/data layout,
distribution hardening flags on stock Lua, and GCC 16.1.1 vs 16.2.1.
Therefore: treat differences below about 5% as noise, and do not read
the 13-16% rows as a Luar regression until they are reproduced with
identical builds of both runtimes.

### The interpreter gap to LuaJIT

LuaJIT 2.1's interpreter (`-joff`) is not the Lua 5.5 VM.  It is already
1.64x faster than stock Lua across this corpus, and 2-3.5x faster on
hash tables, coroutines, varargs and `string.format`; it is slower on
`string/concat` (0.38x the throughput of Lua 5.5, because LuaJIT interns
all strings) and roughly equal on `gsub`.  So a significant part of the
LuaJIT advantage is interpreter quality and data-structure choices, not
just the JIT.

Luar 0.1 inherits the Lua 5.5 interpreter unchanged, so it starts from
the stock-Lua position.  A Luar JIT has to recover both the ~1.6x
interpreter gap where LuaJIT's interpreter wins **and** the additional
JIT gain on top.  In particular, closures, method dispatch, varargs and
`table/hash` are cases where LuaJIT's interpreter is 1.3-2.6x faster
before any machine code is generated.

### What the JIT must recover

The gap to close is per-workload, and strings are not one workload.
`Luar gap` is Luar best time divided by LuaJIT JIT-on best time: the
factor by which Luar would have to improve to match LuaJIT 2.1 on this
machine.

| workload | LuaJIT JIT-on speedup vs Lua | Luar gap to LuaJIT | reading |
| --- | ---: | ---: | --- |
| numeric/int_sum | 6.21x | 5.4x | hot integer loop; tier-1 target |
| numeric/float_loop | 8.89x | 8.4x | hot FP loop; tier-1 target |
| numeric/fib | 9.79x | 10.0x | call-heavy recursion; needs inlining/guard elimination |
| numeric/mandel | 15.32x | 16.0x | tight FP branch loop; biggest numeric target |
| table/array | 2.80x | 2.8x | allocation + indexed access |
| table/hash | 3.06x | 3.1x | string-key hashing; LuaJIT's interpreter already helps |
| table/new_clear | 7.03x | 5.4x | Luar has the extension, but LuaJIT's JIT compiles the fill |
| string/concat | 0.45x (LuaJIT slower) | 0.46x (Luar already faster) | C-level string building; JIT not the answer |
| string/format | 4.97x | 5.4x | mostly C call overhead/frame handling |
| string/gsub | 1.00x | 1.1x | C pattern matcher; JIT buys almost nothing |
| string/pack | n/a | n/a | absent in LuaJIT 2.1 |
| object/closures | 28.67x | 28.1x | closure calls; inline caches + inlining required |
| object/method | 41.90x | 40.8x | method lookup + call; the largest single gap |
| object/metatable | 31.96x | 37.2x | `__index` call; needs inline-cache/metamethod fast path |
| coroutine/ping_pong | 3.37x | 3.5x | yield/resume must be cheap; interpreter work too |
| varargs/select | 16.92x | 19.1x | vararg packing + `select`; JIT-friendly target |

This is the performance contract the JIT will be judged against.  It
would be dishonest to promise those factors from a first tier: the object
rows (28-42x) and the varargs/fib rows (10-19x) require inlining,
metamethod inline caches and good call-side handling, not just arithmetic
code generation.  The more realistic early wins are the numeric loops
(5-16x) and `table/array`/`table/hash` (3x), with coroutines (~3.5x)
dependent on a redesign of resume/yield that a pure trace JIT does not
provide by itself.  Strings show where a JIT should *not* be spent:
`gsub` is flat, `concat` is already slower under LuaJIT, and `format` is
a C-call boundary workload.

## Repeatability and noise

- **Sampling:** 28 CPU samples per row (4 passes x 7), best and median
  reported.  Best-of-N is an optimistic lower bound; the median is the
  central value under throttling.  Treat 1.00-1.05x as equal.
- **Fresh processes:** each benchmark starts a new interpreter, avoiding
  cross-benchmark GC/JIT/string-table interactions.  Without this, a
  preceding `table/hash` made LuaJIT's `string/concat` appear ~15x
  slower; that was a harness artefact, not a LuaJIT property.
- **A targeted repeatability check** (5 independent process runs, 9
  samples each) gave stable ratios for the largest Luar/Lua gaps:
  `int_sum` 0.885 (0.87-0.89), `metatable` 1.17-1.21,
  `gsub` 1.13-1.14, `pack` 1.09-1.10.  `table/hash` is noisier
  (0.84-1.00 across full runs) because its cost depends on GC state;
  its 4-pass result is 1.00 and should be treated as parity.
- **Thermal drift:** the host throttles 10-20% and was shared with another
  agent's builds and desktop applications.  Even passes reverse the
  interpreter order, and the four passes are pooled.
- **Build sensitivity:** see the control build above.  A different
  compiler, optimisation level, PIE/hardening flags or link order can
  move individual microbenchmarks by 10-20% while the core sources are
  identical.  Do not compare these absolutes with another machine.
- **Wall time** is informative but not used for ratios: it includes
  scheduling, frequency changes, GC in the workload and the spawn of the
  closing timestamp process (~1-2 ms).  For allocation-heavy
  `table/hash` it ran at roughly 1.9-2.1x the best CPU time; for compute
  loops it was ~1.05-1.15x.

## Artefacts and reproduction

- Report: `tests/tmp/bench-results-4pass.md` (not committed).
- Raw JSON and per-run logs: `tests/tmp/bench.1JqOnC/` (not committed;
  `KEEP=1` retains them).
- Corpus and harness: `bench/bench.lua`; methodology: `bench/README.md`.
- Reproduce a similar run with:

```sh
make build
REPEAT=7 WARMUP=3 PASSES=4 ./bench/compare.sh
```

`./bench/compare.sh` by default uses 2 passes and writes
`tests/tmp/bench-results.md`.  The exact numbers above are a snapshot:
other builds, other interpreters on `PATH`, or a different thermal state
will move them.  Regenerate rather than quoting them as constants.

## JIT-on/JIT-off snapshot (2026-11-21, 0.72.0)

`make bench-jit` compares the `j4` group with `LUAR_BENCH_NOJIT=1`
(`REPEAT=3 WARMUP=1 SCALE=0.05`).  The numbers are a report, not a gate.

```
benchmark                           jit      nojit  speedup
j4/call_exit                      0.005      0.002     0.45x
j4/call_leaf                      0.000      0.002     9.27x
j4/call_twoarg                    0.000      0.002     8.04x
j4/ffi_ccall0                     0.006      0.010     1.70x
j4/ffi_ccall1dd                   0.002      0.006     4.19x
j4/ffi_ccall1dd_cgetf             0.009      0.008     0.86x
j4/ffi_ccall2dd                   0.002      0.006     3.68x
j4/ffi_ccall2di                   0.002      0.006     3.49x
j4/ffi_ccall2ii                   0.007      0.012     1.65x
j4/ffi_ccall3dd                   0.002      0.007     3.51x
j4/ffi_cdata_copy                 0.001      0.010    20.23x
j4/ffi_cdata_read                 0.014      0.015     1.06x
j4/float_table                    0.000      0.001     2.00x
j4/hash_insert                    0.000      0.000     2.62x
j4/int_sum                        0.001      0.003     5.27x
j4/multi_table                    0.001      0.005     7.87x
j4/table_hash_side                0.001      0.002     1.43x
j4/table_read                     0.000      0.001     2.59x
```

`call_exit` is slower with the tracer because the loop still exits at the
CALL and pays the native entry/exit cost.  It is retained as a visible
regression rather than hidden; no hard threshold is applied.
