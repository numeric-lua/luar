# LuaR 0.7 release notes

LuaR is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
finishes the current J4 side-chain depth work and broadens float table
arithmetic.

## Highlights

- **Depth-4 chained side traces**: parent + three child segments, exact
  parent-state handoff, per-Proto node caching and blocked no-progress PCs.
- **Float subtract/divide**: homogeneous float table roots now support
  `s = s - t[i]` (`FSUB`) and `s = s + t[i] / 2` (`FDIVK`), in addition to
  add and multiply.
- New `tests/jit/61_ir_float_arith.lua`; the J4 suite now has 60 files.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```

GitHub Actions covers GCC/Clang, FFI on/off, JIT=0, ASan/UBSan, fuzz,
staged install/pkg-config, shared-library and the official Lua 5.5.1 suite.

## Known limitations

- Side chains still exclude `TSET` and unsupported float shapes.
- Blocked-PC tracking is bitmap-based and conservative.
- Native exit-stub spilling and full macOS shared ABI tests remain.
- LuaR supports only Lua 5.5; no 5.1–5.4 compatibility mode.
