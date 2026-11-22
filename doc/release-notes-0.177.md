# Luar 0.177 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes a cdata int64 array miscompile.

## Highlights

- `int64_t` / `uint64_t` arrays were classified as kind 1 and recorded with
  the 32-bit `CGETI` op. J4 read 4-byte elements from 8-byte arrays, giving
  wrong values after a few iterations.
- Size-8 integer cdata now returns kind `-1`, so the loop stays on the
  interpreter path. `a[i]` still yields a cdata int64 and the sum is
  `500500LL`.
- `tests/ffi/81_int64_array_semantics.lua` compares JIT and interpreter
  results and types.

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
