# Luar 0.186 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends fused int64 cdata writes.

## Highlights

- `a[i] = i` on `int64_t` arrays compiles to `CSETI64` and executes
  natively.
- `a[i] = a[i] - 1` is covered by the existing `CADDI64` negative delta.
- `tests/ffi/89_int64_set.lua` and `tests/ffi/90_int64_sub.lua` check
  values, `alloc=0` and interpreter fallback.

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
