# Luar 0.185 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds fused int64 in-place arithmetic.

## Highlights

- `a[i] = a[i] + 1` (and negative increments) compiles to `CADDI64` and
  executes natively without boxing cdata values.
- The fused shape requires the same array object on both sides; distinct
  source/destination arrays abort the trace.
- `tests/ffi/88_int64_add.lua` checks values, `alloc=0` and interpreter
  fallback; `ffi/86` keeps the non-fusing guard checks.

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
