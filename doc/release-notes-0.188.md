# Luar 0.188 release notes

Lua is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
fused int64 division by constant.

## Highlights

- `a[i] = a[i] // 2` on `int64_t` arrays compiles to `CDIVI64` and executes
  natively.
- Division truncates toward zero, matching cdata int64 semantics
  (`-125 // 2 == -62`).
- `tests/ffi/92_int64_div.lua` checks positive/negative values, `alloc=0`
  and interpreter fallback.

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
