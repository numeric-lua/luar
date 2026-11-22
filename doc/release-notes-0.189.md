# Luar 0.189 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds fused int64 modulo by constant.

## Highlights

- `a[i] = a[i] % 3` on `int64_t` arrays compiles to `CMODI64` and executes
  natively.
- Remainder truncates toward zero, matching cdata semantics
  (`-500 % 3 == -2`).
- `tests/ffi/93_int64_mod.lua` checks negative/positive values, `alloc=0`
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
