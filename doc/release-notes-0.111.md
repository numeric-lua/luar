# Luar 0.111 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds native zero-argument double-return C calls.

## Highlights

- `CCALL0D` calls a `double f(void)` through libffi and writes a Lua float.
- `tests/ffi/42_trace_ccall0d.lua` verifies compilation, execution and
  `f() * 1000 = 2500.0`.
- The test helper now exports `double_ret` and `int_double_sum`.

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

## Known limitations

- `(int,double)->double` remains interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
