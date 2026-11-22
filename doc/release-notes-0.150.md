# Luar 0.150 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds FADDI arguments to three-argument mixed native calls.

## Highlights

- `ddi_scale(v + 1, v + 2, 4)` compiles and executes natively through
  `CCALL3DDI`.
- The two double arguments may be proven by the recorded IR; runtime guards
  still validate the integer argument.
- `tests/ffi/58_trace_faddi_3ddi.lua` checks the result and zero entry
  failures.

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
