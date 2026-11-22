# Luar 0.154 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds `(double, int, double) -> double` native calls.

## Highlights

- `CCALL3DID` uses libffi `double, sint32, double -> double`.
- `tests/ffi/61_trace_ccall3did.lua` verifies
  `did_scale(2.0, 3, 4.0) * 1000 = 24000.0`, native execution and zero
  entry failures.
- `doc/ffi.md` matrix updated.

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
