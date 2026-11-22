# Luar 0.187 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds fused int64 multiply by constant.

## Highlights

- `a[i] = a[i] * 2` on `int64_t` arrays compiles to `CMULI64` and executes
  natively without boxing cdata.
- Multiplication wraps as int64, matching cdata integer arithmetic.
- `tests/ffi/91_int64_mul.lua` checks values, `alloc=0` and interpreter
  fallback.

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
