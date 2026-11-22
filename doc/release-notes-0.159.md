# Luar 0.159 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a performance gate for the fixed `CCALL2FF` path.

## Highlights

- `bench/bench.lua` now includes `j4/ffi_ccall2ff`
  (`hypotf(3.0, 4.0)` over 400k iterations).
- The 0.158 run records **26.08 ms** best; `j4/ffi_faddi_call` is
  24.93 ms.
- `doc/performance.md` notes the new gate.
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
