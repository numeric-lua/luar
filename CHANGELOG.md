# Changelog

All notable changes to LuaR are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow semver for
the runtime, while the Lua language surface follows upstream Lua.

## [Unreleased]

### Added

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
- `ffi.callback` also handles struct-by-value arguments reaching Lua
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
- J2 native loops (x86-64): a hot `for` loop whose body is numeric register
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

The first LuaR release: Lua 5.5.1 only, a LuaJIT-style surface on top, and
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
- Extended `luaL_openlibs` that opens the LuaR extensions for embedders.
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
- `doc/porting-from-luajit.md`; LuaR documents only the current Lua line.

### Known limitations

- The FFI is opt-in and still 0.2-preview: bitfields, preprocessor macros
  and C→Lua callbacks are not implemented (see `doc/ffi.md`).
- No native JIT. Performance is stock Lua 5.5 interpreter performance; see
  `doc/performance.md` for measured numbers against Lua 5.5.1 and LuaJIT 2.1.
- Only Lua 5.5 is supported.  There is no compatibility mode for any older
  Lua version, and LuaJIT's C API/bytecode are not supported.
