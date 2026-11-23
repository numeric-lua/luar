# Luar roadmap

The roadmap is ordered by user value, not by how satisfying the code is to
write. FFI first (it is why people cannot leave LuaJIT), then distribution,
then the JIT tiers. Every milestone has exit criteria that are observable
from outside the project.

Estimates assume one focused engineer; they are ranges, not commitments.

## 0.1 — Foundation (shipped 2026-11-17)

Delivered:

* `src/` rebranded from pristine Lua 5.5.1, `vendor/lua-5.5.1/` snapshot.
* `luar`, `luarc`, `libluar.a`, installable with `PREFIX`, `luar.pc`, man page.
* `luar` identity table, `LUAR_INIT`, `luar/5.5` module paths.
* LuaJIT-style surface: `jit` (truthful `status()`), `table.new/clear`.
* 5.5-only build: all `LUA_COMPAT_*` switches and legacy APIs removed;
  `global` is always reserved, deprecated math functions are gone.
* Extended `luaL_openlibs`; stock Lua 5.5 C module ABI verified.
* Smoke, semantics, compat and CLI/ABI suites; docs and decision records.

Exit criteria (met): `make test` green; semantics suite identical under stock
`lua` 5.5.1; `luar -v` identifies the Lua base.

## 0.2 — FFI core and J4 foundation (released 2026-11-18)

Scope: `ffi.cdef` (typedef/struct/union/enum/pointer/array/function types),
ctype introspection (`typeof`, `istype`, `sizeof`, `alignof`, `offsetof`),
cdata allocation and access (`new`, `cast`, field/array access, pointer
arithmetic, int64/uint64), `string`/`copy`/`fill`/`gc`/`metatype`, `ffi.C`
and `ffi.load` with fixed and variadic calls through libffi, `ffi.errno`.

Status: implemented behind `WITH_FFI=1`: cdata bitwise/shift, variadic
calls, callbacks (including `cb:set`/`cb:free`, struct/union returns and
packed-argument callbacks on x86-64), structs with array members and unions
of any size up to 16 bytes or larger memory-class shapes by value,
struct/union bitfields, packed/aligned layouts (including packed integer
calls/returns and mixed packed/scalar calls on x86-64), `#pragma pack`,
object/function/variadic cdef macros with stringification/pasting and
local/environment includes, `sizeof`/`_Alignof`/`offsetof` constants, TLS
globals, ASan/UBSan/fuzz coverage and a real `libluatorch.so` consumer
(2.5.1).  What remains: non-x86-64 hosted packed thunks and full macOS ABI
verification.

J4 landed ahead of the original 0.4/0.5 plan: typed tracing IR, live
integer/table roots, interior array→hash side traces, guarded leaf-call
inlining and interpreter safepoints are available behind `jit.opt.start("ir=1")`.

Exit criteria:

* A pure-FFI program that allocates structs, passes pointers to libc and
  calls `qsort`-style APIs runs correctly under `make WITH_FFI=1`.
* No crashes under the ffi test suite with ASan/UBSan (`make test-asan`).
* Documented limitations: packed by-value shapes outside the x86-64
  thunk matrix, and non-x86-64 hosts.
* `make WITH_FFI=0` remains green.

Estimated: 2–4 weeks.

## 0.3+ — incremental J4/FFI releases (0.50–0.81, shipped)

The original 0.3/0.4 plan was delivered as small, independently verified
releases rather than one cut:

* **Native C calls (0.50–0.55, 0.68, 0.73–0.74)**: zero/one/two/three
  argument scalar calls, int/double/float32 and mixed forms, cdata-read
  arguments, guarded `GETUPVAL` function loading, callback rejection.
* **Allocation/GC (0.59–0.67, 0.75–0.81)**: rooted `L->top` during traces,
  stack-offset restore, base reloads, deferred `jit.flush()`, relocated
  TSET base, empty/sized `TNEW`, `SETLIST`/`TSETI`, `GETI`/`SETI`,
  string-key int/float/string constructors and field reads.
* **Performance (0.71–0.73)**: `make bench-jit` JIT-on/off report and a
  committed snapshot.

Exit criteria for this tier: all local matrices + ASan + upstream suite
green; every release tagged, asset-checksum verified, and CI green on
Linux and macOS.  These are met through 0.81.0.

Still outside this tier: general `B == 0` calls, varargs/aggregate C
signatures, strings/closures/`ffi.new` allocation, and sinking.  These
require the allocation-safepoint design in
`doc/allocation-gc-traces.md`, not more edge-case folding.

## 0.3 — Distribution and ABI hardening (released 2026-11-19)

* `libluar.so` exporting the Lua 5.5 C API, with a documented soname and
  `pkg-config` file; static and shared installs tested on Linux and macOS.
  Shared build, `libluar.so.0` soname, explicit export allowlist and the
  `luar.pc` shared/static recipes are in place and tested on Linux
  (`make test-shared`); macOS builds a `.dylib` in the optional CI job; small/HFA/mixed and large by-value unions plus packed aggregate
  arguments/returns/callbacks and TLS now run on arm64.
* CI matrix: gcc/clang, WITH_FFI on/off, JIT=0, staged install with
  `pkg-config`, shared-library FFI, ASan/UBSan and fuzz jobs.  The official
  Lua suite job is optional because CI downloads its inputs.
* Reproducible release tarball (`make dist`/`test-dist`), `make install`/
  `make uninstall` verified.

Exit criteria: a downstream C extension built against `luar.pc` links and
runs against both the static and shared library; CI is green on Linux/macOS.

Estimated: 2–3 weeks.

## 0.4 — Hot loops and trace IR (recorder only)

Status: J1 complete.  Hot counters, trace selection, bytecode snapshots,
`jit.stats()`/`jit.trace()`/`jit.traces()`/`jit.ir()` and
`jit.flush()`/`jit.off()` wiring are implemented, with differential tests
and a deterministic fuzzer.  The linear IR dump is not yet an SSA IR; J2
compiles the whitelisted numeric loops directly from the bytecode snapshot.


* Backedge counters on `Proto` (tagged `/* LUAR */`, reset on `jit.flush`).
* Trace selection, recording of the executed bytecode path with type
  feedback, trace abort/blacklist rules.
* Linear SSA IR with typed values, guards, helper calls and exits; an IR
  dumper (`jit.dump("ir")`-style) that does not require codegen.
* `jit.status()` reports the J2 engine truthfully; `jit.stats()` reports
  loops, traces, aborts, compiled loops and machine-code bytes.
* Differential harness: recorded traces are replayed on the interpreter and
  compared for thousands of generated inputs.

Exit criteria: a corpus of loops produces stable traces; recording overhead
when a loop is hot is bounded and can be disabled; no semantic change.

Estimated: 3–5 weeks.

## 0.5 — x86-64 trace JIT MVP

Status: partial J2.  DynASM emission, entry tag guards, exact FORLOOP
semantics and the VM hand-off exist for `for` loops and simple `while`
loops (up to two per Proto) whose body is numeric register arithmetic (MOVE/LOADI/LOADF/LOADK/ADDI/ADD/SUB/MUL/DIV, the K
variants, integer floor division/modulo, integer bitwise, variable and
constant shifts, unary minus, numeric `and`/`or` tests and MMBIN), with
int/double operands mixed through conversions.  Float-controlled numeric
`for` loops (including branch bodies), constant-index array reads (`t[1]`)
and the 5.5 split-array table representation are handled natively too.
The remaining iterations run natively on the Lua stack slots:
`numeric/int_sum` is ~9x faster than the interpreter (ahead of LuaJIT),
`numeric/float_loop` ~4x, and prefix/square-sum `for`/`while` loops
(including `while` squares and float `while` sums/squares) are register-cached
too.  Numeric branches and simple `while` loops are
in; general traces/side exits, calls and table writes are still missing.  See
`doc/adr/0004-j2-loop-level.md` for the scope decision.


* DynASM-based codegen for the numeric core of T1 loops: integer and float
  arithmetic, comparisons, `FORLOOP`/`FORPREP`, `JMP`, `MOVE`, locals,
  constants, returns.
* Guards for value tags and integer/float subtypes; guard-exit stubs that
  resume the interpreter at the right PC.
* Safe points around calls/allocations; write barriers for table stores.
* `jit.on/off/flush/status` become real; `jit.opt` controls hot thresholds.
* `jit.status()` returns `true` and the LuaJIT-compatible flags string.

Exit criteria:

* Numeric benchmarks (loops, fib, mandelbrot) reach at least 3× stock Lua
  5.5.1 measured by `bench/`, with all semantics tests and the differential
  harness green.
* Runaway compilation is bounded (trace length caps, blacklists).
* A `jit.flush()` during execution never leaves stale code reachable.

Estimated: 6–10 weeks.

## 0.6 — Trace coverage (partial)

Status: J3 table loops remain opt-in behind `jit.opt.start("table=1")`;
default raw fill/sum intrinsics cover the common array shapes.  J4 now adds
an opt-in tracing IR (`ir=1`) with integer/table roots, K-ops, multi-table
bodies, guarded leaf-call inlining, and interior array→hash side traces.
Generic table shape guards and non-inlineable call exits remain open.  The
Luar-only `j4` benchmark group measures this path without implying that it
is faster than J2/J3 yet.

* Table loads/stores with shape guards and inline caches, `GETFIELD`/
  `SETFIELD`/`GETI`/`SETI`, array-part fast paths.
* Calls: direct calls to Lua functions, helper-call fallback, return
  handling, recursion depth checks; method calls (`OP_SELF`).
* Metamethod guards (`OP_MMBIN*`) with correct fallback ordering.
* Side traces for exits; trace trees; loop nesting.
* Coroutines: per-thread recorder state, yields as exits; `OP_CLOSE`/`TBC`.

Exit criteria: table/object/coroutine benchmarks show 1.5–3× over stock Lua,
and the differential harness covers every new opcode family.

Estimated: 8–12 weeks.

## 0.7 — FFI in traces

* Inline cdata loads/stores, struct field offsets, array bounds guards.
* Direct C calls for common signatures, bypassing libffi when statically
  known; `ffi.C` symbol cache.
* Allocation sinking for cdata created and discarded inside a trace.

Exit criteria: an FFI numeric kernel (image/array processing) matches or
beats LuaJIT 2.1 within 30%, with identical results and no leaks.

Estimated: 6–10 weeks.

## 0.8 — Lua 5.6 readiness

Lua 5.6 is not released yet (no work versions as of 2026-11). Preparations:

* Keep version-specific knobs in one place (`LUAR_LUA_VERSION`, module dirs,
  bytecode version tests).
* Track the 5.5→5.6 diff as it appears; keep the semantics suite split by
  version so both can run during transition.
* Prototype the sync on a scratch branch: vendor 5.6, rebase `src` deltas,
  run the official suite, catalogue breakage.

Exit criteria: a dry-run 5.6 sync completes with a written report of every
breaking change and the required Luar work.

## 1.0 — Stable Lua 5.6-forward runtime

* Adopts Lua 5.6 as the only supported language line; 5.5 moves to a
  maintenance branch for one release cycle.
* JIT covers numeric, table, call and FFI workloads with a documented
  performance envelope; interpreter fallback always available.
* API stability promise: `luar_*` C functions and the LuaJIT-compatible
  extension surface follow semver; the Lua language surface follows Lua.
* Packaging: signed tarballs, distro recipes (Arch/AUR, Homebrew), CI matrix,
  reproducible builds, a migration guide from LuaJIT.

## Sequencing rules

1. Correctness beats speed. A JIT that changes observable behaviour is a
   regression, not a feature.
2. No milestone ships with a misleading `jit.status()` or `luar.features`
   entry.
3. Every milestone updates `doc/compatibility.md` and `bench/` numbers.
4. Upstream adoption is never deferred past one Lua minor release.
