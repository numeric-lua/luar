# Changelog

All notable changes to LuaR are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow semver for
the runtime, while the Lua language surface follows upstream Lua.

## [0.1.0] — 2026-11-17

The first LuaR release: Lua 5.5.1 underneath, LuaJIT-shaped surface on top,
and the machinery to keep following Lua.

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
- LuaJIT-compatible `bit` library, verified value-for-value against
  LuaJIT 2.1 (32-bit folding, round-to-nearest-even, shifts, rotations,
  `tohex`, variadic arity errors).
- `table.new` and `table.clear`.
- FFI (0.2 preview, `WITH_FFI=1`): `cdef` parser, C type model and layout,
  cdata allocation/access, int64/uint64, metatypes and finalizers,
  `ffi.C`/`ffi.load` calls through libffi (struct-by-value included),
  `ffi.errno`, plus `type()`/`tonumber()` integration and LuaJIT NULL
  semantics.
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
- `doc/compatibility.md` — the compatibility contract, LuaJIT API matrix,
  deliberate 5.1 omissions.
- `doc/porting-from-luajit.md` — practical migration guide.
- `doc/testing.md` — suite layout, differential testing, sanitizers.
- `doc/upstream.md` — vendor/baseline policy and the Lua 5.6 adoption
  checklist.
- `doc/adr/0001`–`0003` — core choice, libffi, upstream deltas.

### Tests and tooling

- Semantics suite that also passes under stock Lua 5.5.1.
- Differential suite: identical stdout and exit status under `lua` and
  `luar`.
- Official Lua 5.5.1 test suite runner (`tests/upstream/run.sh`); basic mode
  passes 31/31 files and the canonical `final OK !!!`.
- CLI/ABI/embedding tests, and `tools/upstream-check.sh` +
  `tools/upstream-sync.sh` with a checked-in delta baseline.
- GitHub Actions CI for Linux (and an optional macOS job).

### Known limitations

- The FFI is opt-in and still 0.2-preview: variadic calls, bitfields,
  preprocessor macros and C→Lua callbacks are not implemented (see
  `doc/ffi.md`).
- No native JIT. Performance is stock Lua 5.5 interpreter performance; see
  `doc/performance.md` for measured numbers against Lua 5.5.1 and LuaJIT 2.1.
- Lua 5.1 APIs and LuaJIT C API/bytecode are deliberately unsupported.
