# Luar 0.21 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes constant writes for scalar struct/union fields.

## Highlights

- **Constant double field writes**: `p.y = 2.5` is recorded as
  `CSETFIELDK` with float kind and executes natively.
- Constant-only float traces are accepted; the previous floatify gate
  required a live float slot.
- `tests/ffi/22_trace_cdata.lua` now covers constant integer and constant
  double field writes.

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

GitHub Actions also runs the macOS shared/install/module job.

## Known limitations

- Nested aggregate/pointer field writes still fall back.
- C calls and complex cdata values remain interpreter-side.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
