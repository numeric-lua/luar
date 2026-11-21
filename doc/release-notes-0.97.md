# Luar 0.97 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends cdata traces to safe struct/pointer field reads.

## Highlights

- `int *p` reads and `int`/`double` struct fields can trace.
- 64-bit field reads (`long`, `int64_t`) are rejected in both `integer_only`
  and `floatify`; they remain interpreter-side because they produce cdata
  int64 and J4 has no cdata arithmetic.
- `tests/ffi/39_trace_cdata_struct_fields.lua` covers int-pointer arrays,
  int/double struct fields and a `long` fallback.

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

## Known limitations

- 64-bit cdata arithmetic is still unsupported in traces.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
