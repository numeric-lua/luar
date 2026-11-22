# Luar 0.182 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a performance gate for mixed integer/float widening.

## Highlights

- `bench/bench.lua` now includes `j4/ffi_ccall2ff_mixed`
  (`hypotf(i, x)` over 400k iterations).
- 0.181 run: **28.01 ms** best, close to `j4/ffi_ccall2ff_loop`
  (26.57 ms).
- `doc/performance.md` records the new gate.
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
