# Luar 0.153 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a performance gate for FADDI-backed native calls.

## Highlights

- `bench/bench.lua` now includes `j4/ffi_faddi_call`
  (`fabs(v + 1)` over 400k iterations).
- The 0.152 run records **20.55 ms** best, ahead of `ffi_ccall3dd`
  (32.52 ms) and `ffi_ccall2ii` (131.54 ms).
- `doc/performance.md` notes the new gate.

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
