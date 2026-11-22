# Luar 0.152 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes FADDI coverage across mixed two-argument native calls.

## Highlights

- `double_float_scale(2.5, v + 1)` compiles and executes natively.
- Both float arguments may now be proven by recorded IR slots, independent
  of bytecode load order.
- `tests/ffi/60_trace_faddi_2df.lua` checks the result and zero entry
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
