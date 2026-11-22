# Luar 0.176 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a performance gate for widened loop calls.

## Highlights

- `bench/bench.lua` now includes `j4/ffi_ccall2ff_loop`
  (`hypotf(i, i)` over 400k iterations).
- The 0.175 run records **30.70 ms** best, close to the constant-argument
  `j4/ffi_ccall2ff` (31.31 ms).
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
