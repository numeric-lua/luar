# Luar 0.19 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes scalar struct/union field access for FFI-in-trace.

## Highlights

- **Struct field writes**: `CSETFIELD` stores signed integer and `float`/
  `double` values into cdata struct/union fields.
- **Struct field reads** remain covered by `CGETFIELD`.
- Field offsets/sizes are resolved at record time; constant-RK writes,
  bitfields, pointer fields, nested aggregates and unsupported types fall
  back.
- `tests/ffi/22_trace_cdata.lua` now covers array reads/writes, struct reads
  and struct writes in one FFI-trace matrix.

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

- Struct writes require a register value (constant-RK writes fall back).
- C calls and complex cdata values remain interpreter-side.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
