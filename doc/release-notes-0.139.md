# Luar 0.139 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes FADDI arguments to native C calls.

## Highlights

- `FADDI` was absent from `ir_ins_is_float`; `fabs(v + 1)` therefore chose
  `CCALL1DI` and exited at its integer argument guard (PC 9).
- It now chooses `CCALL1DD`, executes natively and returns `3500.0` for
  1000 iterations with `ir_entry_failures = 0`.
- `tests/ffi/40_trace_float_addi_call.lua` now requires native execution
  and zero call exits.

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
