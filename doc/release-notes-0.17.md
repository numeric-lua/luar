# Luar 0.17 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes the numeric cdata read/write subset for FFI-in-trace.

## Highlights

- **Cdata float/double writes**: `CSETF` stores Lua float values into
  `float[?]`/`double[?]` arrays and pointers, with bounds, null-pointer,
  element-size and value-type validation in a no-allocation helper.
- Copies such as `dst[i] = src[i]; s = s + dst[i]` stay in J4 through
  `CGETF`/`CSETF`.
- `tests/ffi/22_trace_cdata.lua` covers int reads, int writes, double reads
  and double copies.
- Unsupported cdata shapes still fall back.

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

- Struct fields, C calls and complex cdata values remain interpreter-side.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
