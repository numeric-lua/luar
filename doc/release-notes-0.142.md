# Luar 0.142 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends FADDI arguments to three-argument native calls.

## Highlights

- `fma(v + 1, v + 2, 3.0)` compiles and executes natively.
- `CCALL3DD` accepts three IR-proven float slots in addition to the
  bytecode argument proof.
- `tests/ffi/51_trace_faddi_3arg.lua` checks the result and zero entry
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
