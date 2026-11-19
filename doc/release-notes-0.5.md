# LuaR 0.5 release notes

LuaR is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the opt-in J4 tracing-IR path with chained side execution and closes
mixed int/float shape correctness.

## Highlights

- **Chained J4 side traces**: when a parent side fails at an exact PC beyond
  its start, LuaR compiles and executes a child side from that PC, reusing the
  parent’s mutated state.  Child nodes are depth-tracked and cached; deeper
  chains remain capped.
- **Mixed int/float table correctness**: int and float root specializations
  no longer corrupt each other when the same Lua function sees different cell
  types.
- **Exact root/side exit PCs**: guard and helper failures resume at the
  failing instruction rather than the loop head, preventing double-counted
  prefixes.
- New `tests/jit/57_ir_guard_exact.lua`, `58_ir_mixed_cells.lua` and
  `59_side_chain.lua`.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make test-fuzz
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```

GitHub Actions runs the GCC/Clang, FFI on/off, JIT=0, ASan/UBSan,
deterministic fuzz, staged install/pkg-config, shared-library and official
Lua 5.5.1 suite jobs.

## Known limitations

- Chaining currently covers one child (depth 2); depth 3–4 and blocked-start
  metadata remain future work.
- Side segments reject `TSET` and unsupported float arithmetic and fall back.
- Homogeneous float table cells are supported; mixed cells use interpreter
  fallback.
- Native-ABI exit-stub spilling and full macOS shared ABI tests remain.
- LuaR supports only Lua 5.5; there is no 5.1–5.4 compatibility mode.
