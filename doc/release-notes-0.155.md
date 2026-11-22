# Luar 0.155 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds `(int, double, double) -> double` native calls.

## Highlights

- `CCALL3IDD` uses libffi `sint32, double, double -> double`.
- `tests/ffi/62_trace_ccall3idd.lua` verifies
  `idd_scale(3, 2.0, 4.0) * 1000 = 24000.0`, native execution and zero
  entry failures.
- With `CCALL3DD`, `CCALL3DDI`, `CCALL3DID` and `CCALL3IDD`, all simple
  three-argument permutations with one integer and two doubles are covered.

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
