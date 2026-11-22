# Luar 0.145 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends FADDI arguments to `double(int,double)` native calls.

## Highlights

- `ldexp(v + 1, 2)` compiles and executes natively through `CCALL2DI`.
- The first argument may be proven float by the recorded IR; the integer
  argument keeps its bytecode proof.
- `tests/ffi/53_trace_faddi_2di.lua` checks the result and zero entry
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
