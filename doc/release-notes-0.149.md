# Luar 0.149 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds `(double,double,int) -> double` native calls.

## Highlights

- `CCALL3DDI` uses libffi `double, double, sint32 -> double`.
- `tests/ffi/57_trace_ccall3ddi.lua` verifies
  `ddi_scale(2.0, 3.0, 4) * 1000 = 24000.0`, native execution and zero
  entry failures.
- Integer literals in the double parameters remain interpreter-side; the
  two double-typed slots must be float-tagged.

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
