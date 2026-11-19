# Changelog

All notable changes to Luar are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow semver for
the runtime, while the Lua language surface follows upstream Lua.

## [Unreleased]

## [0.18.0] — 2026-11-19

### Added

- `CGETFIELD`: scalar signed integer and `float`/`double` struct/union field
  reads in J4, with bitfield rejection and bounds checks.
- `tests/ffi/22_trace_cdata.lua` now covers struct int and double fields.

## [0.17.0] — 2026-11-19

### Added

- `CSETF`: float/double cdata array and pointer writes in J4.
- `tests/ffi/22_trace_cdata.lua` now covers double copies.

## [0.16.0] — 2026-11-19

### Added

- `CSETI`: integer cdata array/pointer writes in J4, with bounds/type checks
  and no allocation.
- `tests/ffi/22_trace_cdata.lua` now covers int reads, int writes and double
  reads.

## [0.15.0] — 2026-11-19

### Added

- `CGETF`: double/float cdata array and pointer reads in J4, with float
  accumulation through the existing float specialization.
- `tests/ffi/22_trace_cdata.lua` now covers both int and double arrays.

## [0.14.0] — 2026-11-19

### Added

- FFI-in-trace subset: integer cdata array/pointer elements read through
  `CGETI` with an identity guard and bounds/type validation.
- `tests/ffi/22_trace_cdata.lua`; unsupported cdata shapes fall back.

## [0.13.0] — 2026-11-19

### Added

- Side-segment hash insertion coverage (`tests/jit/67_side_hash_insert.lua`).
- `j4/hash_insert` benchmark from the 0.12 cycle.

## [0.12.0] — 2026-11-19

### Added

- Hash insertion/rehash through J4 `TSET` with an L-aware loop ABI.
- `tests/jit/66_hash_insert.lua` and the `j4/hash_insert` benchmark.

## [0.11.0] — 2026-11-19

### Added

- Existing-cell J4 side `TSET` on tables with a metatable, while empty cells
  still honor `__newindex`.
- macOS C-module compile/load/require check in CI.
- `tests/jit/65_side_meta_tset.lua`.

## [0.10.0] — 2026-11-19

### Added

- Existing-hash-cell integer `TSET` in J4 side segments, without allocation
  or metamethod fallback.
- `tests/jit/64_side_hash_tset.lua` covering hash writes and reads.

## [0.9.0] — 2026-11-19

### Added

- `TSET` in chained J4 side segments, using exact-PC fallback instead of
  replaying an already-mutated prefix.
- `tests/jit/63_side_tset.lua` covering a side that performs a table write.

## [0.8.0] — 2026-11-19

### Added

- Full-width (`LUAR_IR_MAX_SLOTS` = 64) exit snapshot/restore selftest.
- Growing-write/TSET fallback regression test.
- macOS shared-dylib link/run and staged-install/pkg-config checks in CI.

## [0.7.1] — 2026-11-19

### Changed

- Renamed all display branding from “LuaR” to **Luar**.  Executables,
  modules, C symbols and `LUAR_*` macros are unchanged.

## [0.7.0] — 2026-11-19

### Added

- J4 float subtraction (`FSUB`) and division by constant (`FDIVK`).
- `tests/jit/61_ir_float_arith.lua`.
- Depth-4 chained side traces and blocked no-progress tracking from the
  0.6 cycle.

## [0.6.0] — 2026-11-19

### Added

- J4 side chains now reach depth 4 (parent + three children), reusing exact
  failure PCs and parent state.
- Blocked/no-progress PC tracking for side chain children.
- `tests/jit/60_side_chain_depth.lua` covering a four-closure chain.

## [0.5.0] — 2026-11-19

### Added

- Chained J4 side traces: a parent side can compile and execute one child
  segment at its exact failure PC, with depth-2 tracking and reuse.
- `tests/jit/59_side_chain.lua` covering closure-identity chaining.
- Exact side-failure PCs and mixed int/float root correctness from the 0.5
  cycle.

### Changed

- J4 side compilation rejects unsupported float shapes rather than emitting
  integer code for them.

## [0.4.0] — 2026-11-19

### Added

- Live homogeneous float-table J4 roots (`TGETF`, `FMULK`, `FADD`), including
  float constants such as `t[i] * 2.0`.
- J4 CALL-exit prefixes: non-inlineable calls execute the natively supported
  prefix, resume the interpreter at the CALL, and continue the loop.
- J4 benchmarks for float tables and CALL exits.

### Changed

- J4 table traces keep tag guards, including collectable table tags.
- Integer J4 roots reject table-typed arithmetic operands and float constants
  that have no integer specialization; such traces fall back safely.
- J4 helper failures preserve their exact exit PC across C helper calls.

### Fixed

- `x = t[i] * 2.0` no longer loses the double constant (integer `imul 0`).
- J4 stats now distinguish CALL exits (`ir_call_exits`).

## [0.3.0] — 2026-11-19

### Added

- Luar-only `j4` benchmark corpus covering integer, table-read,
  array→hash side-trace, one- and two-argument leaf-call, and multi-table
  paths; `bench/compare.sh` reports stock Lua/LuaJIT as skipped for `j4`.
- Two-argument integer leaf-call inlining in J4 roots.
- J4 benchmark and correctness coverage for exact side-failure resume,
  closure-guard fallback, cycle classification, `__newindex` fallback and
  table-write metatable rejection.
- J4 table traces now keep guards (collectable table tags) and report a
  per-instruction resume PC; native exit PCs are bounds-checked in `lvm`.
- The shared-library export map is an explicit public `luar.h` allowlist;
  macOS builds use `.dylib` names and `@rpath` install names.
- `make dist` requires a clean tree; staged install CI compiles and runs
  shared and static consumers through the installed `luar.pc`.

### Changed

- J4 side linking now records the root start separately from the native
  backedge for correct cycle classification.
- Side-code cache exhaustion no longer leaks generated code.
- Benchmark documentation no longer describes Luar as interpreter-only.

## [0.2.0] — 2026-11-18

### Added

- The default build level is now `-O3` (overridable with `CFLAGS`); it brings
  the interpreter `mandel` path to stock-Lua parity without changing
  semantics.
- FFI cdata bitwise/shift operators (`& | ~ << >>`, `__band`..`__bnot`),
  `//` and `^` with LuaJIT-compatible 64-bit promotion and masked shifts.
- FFI parser hardening: constant-expression arithmetic wraps in 64 bits,
  `INT64_MIN / -1` and `% -1` no longer trap, and deeply nested declarators
  or constant expressions raise a Lua error instead of overflowing the C
  stack, and function type ids are sized from their parts (no buffer
  overflow for long type names).  Found by the new deterministic cdef
  fuzzer.
- `string.format` integration for cdata: integer conversions and
  `%s`/`%q` follow LuaJIT; plain Lua values are unchanged.
- `ffi.gc(cb, fn)` now runs `fn` for callback cdata before releasing the
  closure, and `string.format` skips its cdata rewrite when no argument is
  cdata.
- Unions of any size up to 16 bytes, including mixed SSE/INTEGER
  eightbytes, odd sizes (9–15) and array/struct members, and structs
  containing them can be passed and returned by value.  Larger unions use
  the memory ABI when their size is a multiple of their alignment;
  oversized ones remain rejected.
- FFI structs with fixed array members (including arrays of structs) can be
  passed and returned by value; VLA/empty arrays and bitfield structs are
  rejected.
- `ffi.cdef` preprocessing: object-like and named/variadic function-like
  macros (`...`/`__VA_ARGS__`), `#undef`, nested argument substitution, and
  `#ifdef`/`#ifndef`/`#if`/`#else`/`#endif`, with macros expanded outside
  strings and comments and persisted across cdef calls.  Function-like
  macros, non-pack `#pragma` and complex `#if` expressions fail clearly;
  `<...>` includes use local resolution.  Token pasting (`##`) and parameter stringification (`#`)
  work in function macros, and `#error` text is expanded; quoted
  `#include "path"` resolves relative to the including file (cwd fallback)
  with include-guard/macro persistence and depth/size limits.  `<...>`
  paths use the same local resolution; there is no system include search.  `#pragma pack(n/push/pop)` sets byte-granular layout.
- Fixed the cdef preprocessor dropping the last character of indented lines
  (leading whitespace was subtracted from the expansion length twice).  This
  affected multiline enums and any indented declaration followed by commas.
- `ffi.cdef` constant expressions accept `sizeof(type)`, `_Alignof`,
  `offsetof` and `__builtin_offsetof`, including arrays and use inside
  array bounds/enums.
- `ffi.istype` follows LuaJIT pointer compatibility: a struct/union accepts
  a pointer to it and same pointer types match; `void *` has no special case.
- `ffi.offsetof` reports LuaJIT's bitfield form: storage-unit byte offset,
  bit position within the unit and width; byte-packed structs report the
  containing byte and bit offset within it.
- Callback cdata supports LuaJIT's `cb:set(function)` and `cb:free()`
  methods; freeing invalidates the pointer and later use is a clear error.
- `__attribute__((packed))`/`aligned(n)` are accepted before the struct tag
  as well as after the body; byte-granular/over-aligned layout and field
  access work.  On x86-64, a packed integer struct with one by-value
  argument and scalar return is called through a MEMORY-ABI stack thunk;
  other packed aggregates are rejected by value rather than mislayed.
- FFI bitfields for integer/enum/`_Bool` struct fields: GCC/x86-64
  bit-granular packing including mixed base types, byte-crossing fields,
  unnamed padding and zero-width alignment fields, signed extraction, masking on set, positional/named
  initializers; struct and union bitfields share the same layout engine,
  and bitfield structs can be passed/returned by value through grouped
  synthetic integer storage fields.
- `__thread`/`_Thread_local` globals are supported through the existing
  dlsym symbol path, with read/write access from Lua.
- `ffi.callback` also returns small scalar unions by value when the Lua
  callback returns a matching cdata.
- `ffi.callback` returns structs by value when the Lua callback returns a
  matching cdata, and also handles struct-by-value arguments reaching Lua
  (covered by the helper's `call_pair_cb`).
- `ffi.callback`: libffi closures that call Lua functions, with GC
  lifetime, error capture through `ffi.callback_error()`, and clear
  rejection of aggregate-return/variadic callback types.
- A real-consumer FFI test: `tests/consumer/luatorch.lua` loads the
  libtorch-backed `libluatorch.so` C ABI and runs tensor creation, `lt_add`,
  scalar reads and data transfer through `ffi.load`.
- Deterministic JIT differential fuzzing (`tests/jit/04_fuzz.lua`): 150
  generated chunks compare interpreter and compiled results bit-for-bit,
  including value flavours and mixed int/float arithmetic.
- JIT differential/stress coverage (`tests/jit/03_guards.lua`): every
  compiled-loop case is compared against an interpreter run, including
  overflow, empty loops, division, negative zero, `break` and type-changing
  call sites.  `make JIT=0 test` verifies the compiled-out configuration.
- J2 `for` loops now support native `break` exits (the compiler's `CLOSE`
  is a no-op when the whitelisted body has no closures); conditional
  `while` backedges remain conservative.
- Build: `-MMD` header dependencies make `make` rebuild when `Proto`,
  JIT or emitter headers change.
- J2 while-loop differential fuzzing (`tests/jit/05_while_fuzz.lua`):
  100 generated stepped loops compare interpreter and native results.
- J2 `while` support: simple integer-conditioned `while` loops (body without
  calls/tables/float conditions) run their remaining iterations natively;
  `break` and float conditions fall back.
- Small, HFA, mixed and large by-value unions plus packed aggregate
  arguments/returns/callbacks and Darwin TLS are enabled and tested on
  arm64.
- The macOS CI job now builds and tests the full FFI suite; `ffi.load`
  resolves `lib*.dylib`, and x86-64-only union/packed/TLS tests are gated
  behind the architecture.
- J3 raw table fill/sum intrinsics (default, `tint=0` disables):
  `t[i]=i` and `acc=acc+t[i]` loops are executed by C helpers, making
  `table/array` ~0.14x stock Lua (and faster than LuaJIT).
- Guard-exit driven loop variants: when a compiled numeric loop fails its
  type guards, a second variant is compiled from the live frame if a slot
  is free and `hotexit` exits have accumulated; successful variants are
  counted by `jit.stats().side_traces` (`jit.opt.start("hotexit=N")`,
  default 10) (int/float polymorphic loops); both variants are tried at each
  backedge.  `tests/jit/25_polymorphic.lua` covers it.
- Inline a simple closure shape: `f` with one integer upvalue doing
  `x = x + arg`, called for its side effect in a numeric loop.  The native
  loop updates the open upvalue directly; `object/closures` drops to ~0.01x
  stock Lua and beats LuaJIT.  Other call shapes still interpret.
- Inline a simple `__index` multiplier shape: an empty table with an
  `__index` function returning `k * N` is compiled to `acc += idx * N`,
  re-validating the table/metatable/function body at every native entry.
  `object/metatable` drops to ~0.01x stock Lua and beats LuaJIT.
- Inline a one-field method shape: `obj:add(i)` where the method body is
  `self.v = self.v + x` is executed by a C loop that computes the final field
  value and stores it once.  Handles table registers and upvalues;
  `object/method` drops to ~0.02x stock Lua and beats LuaJIT.
- Documented the entry-guard placement limitation preventing prefixed table
  loops from producing non-root side exits.
- Live side tests now assert depth and root-cycle classification.
- Documented the entry-guard placement limitation preventing prefixed table
  loops from producing non-root side exits.
- Live side tests now assert depth tracking as well as compilation and
  execution.
- Added guarded inlining of one-argument integer leaf calls in IR roots.
- Declared the J4 mid-loop side-trace milestone complete in the J4 document.
- Added a deterministic IR/side differential test over array/hash shapes.
- Added `__index` fallback coverage for table IR paths.
- Added mixed table/call interior side-trace coverage.
- Added multi-table interior side exits and recorded side depth.
- Added periodic interpreter yields (safepoints) for generated root traces,
  counted by `ir_yields`.
- Fixed `MMBINI` immediate-slot bounds checking, enabling prefixed table
  bodies in IR recording and true interior side exits (asserted by
  `tests/jit/45_ir_interior.lua`).
- Linked live side traces from IR-root table guard exits, including
  snapshot/restore and counters.
- Added live array-to-hash side traces: a guard exit compiles a generic
  hash-`TGET` side segment, executes it with snapshot/restore, and continues
  the loop.  Added `tests/jit/43_side_live.lua`.
- Added a generic hash-lookup primitive for future array-miss side traces.
- Expanded exit snapshots to 64 slots and added a restore selftest.
- Added side-failure/restore coverage for interior segments.
- Added the side-continuation emitter contract: interior segment + FORLOOP
  update returning continue/done/failure, with a selftest.
- Added live table-read and table-write IR execution (`TGET`/`TSET`) with
  table/key validation; array growth and float arithmetic fall back.
- Added IR fallback coverage for table loops and reset `ir_executed` on
  `jit.flush()`.
- Added opt-in live execution of generated root traces (`ir=1`) and fixed the
  DynASM arithmetic action scoping that had dropped the ADD store/tag.
- Added a trace-entry guard contract (`luar_trace_ir_guards_match`) and
  selftest for live-entry validation.
- Added interior-segment IR execution coverage via
  `jit.trace_ir().side_exec_selftest()`.
- Added integer trace-IR execution coverage via
  `jit.trace_ir().exec_selftest()`.
- Added integer trace-IR to x86-64 machine-code emission, compiled as a
  validation shadow (`jit.stats().ir_compiled/ir_code_bytes`).
- Added J4 side-link metadata: depth cap 4, root-cycle detection and IR
  type/liveness analysis.
- Added J4 interior guard-exit IR recording (one segment per Proto) and
  table/K-op IR ops, counted by `jit.stats().ir_side_traces/ir_side_ops`.
- Added a J4 numeric-loop IR recorder wired into the hot hook, counted by
  `jit.stats().ir_traces`, `ir_ops` and `ir_aborts`.
- Added the independent J4 tracing-IR foundation (`luar_trace.c`): typed
  values, guards, exits, a verifier, live-slot exit snapshots/restore and a
  selftest exposed as `jit.trace_ir()`.
- Code slots record their entry guard tags and dispatch to the matching
  variant (`jit.stats().sig_hits`), avoiding calls into a known-mismatched
  variant.
- Mid-loop table guards now return the exact failing bytecode PC (reported as
  `jit.stats().last_exit_pc`) and resume the interpreter at that instruction,
  preserving prior iteration state.  This is side-exit state capture, not a
  recorded side trace.
- Inline branchy two-parameter leaves through the generic integer evaluator.
- Inline four-parameter leaf calls through the bounded evaluator.
- Inline branchy three-parameter leaves through the generic evaluator.
- Inline three-parameter leaf calls (`a*b+c`, `a*b*c`, `a+b+c`, `a-b-c`)
  whose result feeds a numeric accumulator; second and third arguments are
  read from their loop-body slots and guarded as integers.
- Inline simple branch-containing integer leaves: comparison + JMP arms
  (`if x > K then return x*2 else return x+1`), plus a bounded evaluator for
  short branchy bodies with comparisons, JMP, booleans and arithmetic.
- Inline two-parameter straight-line integer leaves feeding a numeric
  accumulator, including `a*b+b`, `a*b`, `a+b` and `a-b`; the second argument
  is read from its loop-body slot and guarded as an integer.
- Inline one-parameter leaf arithmetic calls feeding a numeric accumulator
  (`s = s + f(i)`), including straight-line integer chains with affine
  `x*K+C` fast folding and a bounded general evaluator for
  `x*K`, `x+K`, `x*x`, `x+x`, `(x+K)*K`, squares plus offsets and similar
  leaf bodies.  The closure Proto
  and bytecode are revalidated each entry; `object/leaf_call` runs at ~0.04x
  stock Lua and beats LuaJIT.
- Keyed raw table fill/sum intrinsics (`t[keys[i]] = i`,
  `s = s + t[keys[i]]`) with hash presizing: `table/hash` drops to ~0.43x
  stock Lua.  Keys may be function parameters or closure upvalues.
- The hot-loop hook now recognises a Proto whose first loop is an intrinsic,
  so subsequent loops in the same function can be recorded and intrinsic
  compiled too.  Generic table emission remains opt-in
  (`jit.opt.start("table=1")`): raw integer array
  stores (`t[i] = i`) and accumulation (`acc = acc + t[i]`), including
  register-cached fill/sum fast paths,, with metatable/bounds/tag guards
  that bail before the current iteration.  Both are correct but not yet
  faster than the interpreter, so they are off by default.
- Early J3 coverage: a read-only integer raw-array lookup
  (`t[i]`, or fused `acc = acc + t[i]`) compiles for one table at the body
  start, with bounds/tag guards that fall back before the current iteration.
  It is experimental and not yet faster than the interpreter.
- J4 peephole: `for` loops with `acc = acc * i` keep the accumulator and
  loop counters in GPRs (integer or float accumulator).
- J4 peephole: `for` loops with `acc = acc + i * i` keep acc/index/count
  in GPRs (integer or float accumulator); a direct 20M-iteration
  sum-of-squares runs ~16x faster than the interpreter.
- J4 peephole: `while i < limit do acc = acc + i; i = i + 1 end` keeps the
  accumulator/index/limit in GPRs; prefix-sum while loops run ~12x faster
  than the interpreter in a direct 20M-iteration measurement; the
  increment can be any nonzero immediate (e.g. `i = i + 2`), and a float
  accumulator keeps `acc = acc + i * const` in an XMM register.  The same
  fast path recognises `acc = acc + i * i` (~10x), including float
  accumulators, and product loops
  `acc = acc * i` with integer or float accumulators.
- J4 peephole: `acc = acc + i` integer loops keep the accumulator and loop
  counters in GPRs; `numeric/int_sum` drops from ~5.9 ms to ~3.9 ms (0.11x
  stock Lua, faster than LuaJIT).
- J4 peephole: `acc = acc + i * const` now covers integer and float
  accumulators; integer constants beyond 32 bits are loaded once.
- J4 peephole: `acc = acc + i * const` float loops keep the accumulator in
  an XMM register and the loop counters in GPRs; `numeric/float_loop` drops
  from ~11 ms to ~6.8 ms (0.25x stock Lua).
- J2 float/double comparisons (`<`, `<=`, `>`, `>=`, `==`, `!=`, including
  NaN-false semantics) work in native `if` branches and `while` conditions.
- J1/J2 can keep two native loops per `Proto` (first and most recent),
  with a bloom of attempted backedges; functions with sequential numeric
  loops compile more than one.
- `jit.opt.start("hotloop=N")` configures the hot threshold for newly
  created Protos.
- `jit.dump("t"/"i")` prints recorded bytecode snapshots and linear IR to
  stderr, matching the LuaJIT-style debugging surface without file/function
  sinks.
- J2 branch support: integer comparisons (`<`, `<=`, `>`, `>=`, `==`, `!=`)
  and forward `if`/`else` jumps inside a whitelisted loop body are emitted as
  guarded native branches; float comparisons and backward inner loops fall
  back to the interpreter.
- J2 native loops (x86-64): a hot `for` or `while` loop whose body is numeric register
  arithmetic (MOVE/LOADI/LOADF/LOADK/ADDI/ADD/SUB/MUL/DIV, the K variants
  and MMBIN, with int/double conversions) is compiled with DynASM and runs
  its remaining iterations directly on the Lua stack slots, with tag guards
  and exact FORLOOP semantics.  In-place accumulator peepholes bring
  `numeric/int_sum` to LuaJIT parity (5.9 ms) and `float_loop` to 11.1 ms;
  integer constants used in float arithmetic convert correctly, and
  type-changing loop-carried registers fall back to the interpreter.  `jit.status()` is now `true` and
  `luar.features.jit` is enabled on x86-64; other hosts keep the interpreter.
- J1 trace recorder: hot-loop counters, trace selection, bytecode
  snapshots, `jit.stats()`, `jit.trace()`, `jit.traces()`, a linear IR
  dump (`jit.ir()`), working `jit.flush()`/`jit.off()` recording control.
  No machine code is generated and `jit.status()` remains `false`.
- `make shared` builds `libluar.so.0.1.0` with SONAME `libluar.so.0` and the
  `libluar.so.0`/`libluar.so` symlinks, from separate `.pic.o` objects. An
  ELF export map (`etc/libluar.map`) limits the dynamic symbol table to the
  Lua 5.5 C API and `luar_*`. `make install` installs it next to
  `libluar.a`; `make test-shared` links an embedding host and the CLI
  against it and loads a stock Lua 5.5 C module. `make clean` removes the
  shared library, symlinks and PIC objects.

## [0.1.0] — 2026-11-17

The first Luar release: Lua 5.5.1 only, a LuaJIT-style surface on top, and
the machinery to keep following Lua.

### Added

- `luar` and `luarc` binaries, `libluar.a`, installable with `PREFIX`,
  `luar.pc` and a man page. Bytecode is the official Lua 5.5.1 format and is
  interchangeable with `lua`/`luac` 5.5.1.
- The Lua 5.5.1 core, vendored pristine in `vendor/lua-5.5.1/` and derived
  into `src/`, with the full language: integers, `<const>`/`<close>`,
  `global` declarations, read-only loop variables, `table.create`, and the
  5.5 standard library.
- `luar` identity table (`version`, `lua_version`, `features`, `os`,
  `arch`), `LUAR_INIT`, and `luar/5.5` module directories ahead of the Lua
  ones.
- `table.new` and `table.clear`.
- FFI (0.2 preview, `WITH_FFI=1`): `cdef` parser, C type model and layout,
  cdata allocation/access, int64/uint64, metatypes and finalizers,
  `ffi.C`/`ffi.load` calls through libffi (struct-by-value and variadic
  calls included), `ffi.errno`, plus `type()`/`tonumber()` integration and
  NULL-pointer semantics.
- `jit` control surface: `version`, `version_num`, `os`, `arch`, `status`,
  `on`, `off`, `flush`, `opt.start`. `jit.status()` truthfully returns
  `false`: no native code generator ships in 0.1.
- Extended `luaL_openlibs` that opens the Luar extensions for embedders.
- LuaJIT-compatible CLI: `-j on|off|flush|opt=...|v` and
  `-b [-s] input output` bytecode compilation.
- Stock Lua 5.5 C modules load unchanged (C ABI preserved, exported symbols);
  `tests/cli.sh` builds one with the system headers and runs it.

### Documentation

- `doc/architecture.md` — value model, tiers, why the Lua 5.5 core is the
  semantic authority, upstream-follow design.
- `doc/roadmap.md` — 0.1 → 1.0 milestones with exit criteria.
- `doc/lua55-only.md` — the practical 5.5-only guide: reserved words,
  native bitwise operators, removed legacy names, strict C API.
- `doc/compatibility.md` — the 5.5-only contract and the LuaJIT API matrix.
- `doc/testing.md` — suite layout, differential testing, sanitizers.
- `doc/upstream.md` — vendor/baseline policy and the Lua 5.6 adoption
  checklist.
- `doc/adr/0001`–`0003` — core choice, libffi, upstream deltas.

### Tests and tooling

- Semantics suite that also passes under stock Lua 5.5.1, including a
  native-bitwise-operator suite and a strict-5.5 regression suite.
- Differential suite: identical stdout and exit status under `lua` and
  `luar`.
- Official Lua 5.5.1 test suite runner (`tests/upstream/run.sh`); basic mode
  passes 31/31 files and the canonical `final OK !!!`.
- CLI/ABI/embedding tests, and `tools/upstream-check.sh` +
  `tools/upstream-sync.sh` with a checked-in delta baseline.
- GitHub Actions CI for Linux (and an optional macOS job).

### Removed

- The vendored LuaJIT 2.1 tree (`vendor/luajit/`).  Only DynASM was kept, in
  `dynasm/`, and its backends were ported from the Lua 5.1 `bit` library to
  native Lua 5.5 operators (byte-identical generated code).
- The LuaJIT-compatible `bit` library.  Lua 5.5 has native `&`, `|`, `~`,
  `<<` and `>>`; a 5.1-era shim does not belong in a 5.5-only runtime.
- Every `LUA_COMPAT_*` switch and code path: `LUA_COMPAT_GLOBAL`,
  `LUA_COMPAT_MATHLIB`, `LUA_COMPAT_APIINTCASTS`, the legacy API macros
  (`lua_strlen`, `lua_objlen`, `lua_equal`, `lua_lessthan`,
  `luaL_checkint`, ...), and the deprecated math functions.
- `doc/porting-from-luajit.md`; Luar documents only the current Lua line.

### Known limitations

- The FFI is opt-in and still 0.2-preview: bitfields, preprocessor macros
  and C→Lua callbacks are not implemented (see `doc/ffi.md`).
- No native JIT. Performance is stock Lua 5.5 interpreter performance; see
  `doc/performance.md` for measured numbers against Lua 5.5.1 and LuaJIT 2.1.
- Only Lua 5.5 is supported.  There is no compatibility mode for any older
  Lua version, and LuaJIT's C API/bytecode are not supported.
