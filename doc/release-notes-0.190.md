# Luar 0.190 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds fused int64 bitwise operations.

## Highlights

- `a[i] = a[i] & 255`, `| 256` and `~ 3` compile to `CBANDI64`,
  `CBORI64` and `CBXORI64` and execute natively.
- `tests/ffi/94_int64_bitwise.lua` checks all three ops, `alloc=0` and
  interpreter fallback.

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
