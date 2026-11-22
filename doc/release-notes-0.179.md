# Luar 0.179 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
hardens the 0.177 int64 cdata fix.

## Highlights

- `luar_ffi_trace_cget_int` now rejects 8-byte elements outright instead of
  reading them with the 32-bit integer helper.
- `int64_t[?]` loops continue to return correct cdata int64 values
  (`500500LL`) on the interpreter path.
- Defense in depth against a future classifier regression.
- No intended runtime behavior change.

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
