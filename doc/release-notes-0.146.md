# Luar 0.146 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends FADDI arguments to `(int,double)->double` native calls.

## Highlights

- `int_double_sum(2, v + 1)` compiles and executes natively through
  `CCALL2ID`.
- The double argument may be proven float by the recorded IR; runtime
  guards still validate the integer argument.
- `tests/ffi/54_trace_faddi_2id.lua` checks the result and zero entry
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
