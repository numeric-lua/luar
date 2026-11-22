# Luar 0.180 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes the 32-bit-helper hardening for size-8 cdata integers.

## Highlights

- `luar_ffi_trace_cset_int` now rejects 8-byte elements, matching the
  0.179 read-side rejection.
- `tests/ffi/83_int64_write_semantics.lua` fills `int64_t` arrays under JIT
  and compares every element with the interpreter result.
- No intended runtime behavior change; int64 arrays stay on the interpreter
  path.

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
