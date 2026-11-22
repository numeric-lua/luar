# Luar 0.156 release notes

This release refreshes the performance gate at 0.155.

## Highlights

- `doc/performance.md` records current Lua/LuaJIT/Luar numbers.
- Integer/table-array loops remain far ahead of stock Lua.
- FADDI-backed native calls (20.78 ms) run near the plain double-call
  forms (`ffi_ccall3dd` 31.21 ms, `ffi_ccall2dd` 32.66 ms).
- No runtime behavior change.

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
