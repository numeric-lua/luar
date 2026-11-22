# Luar 0.158 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes `float(float,float)->float` native calls.

## Highlights

- Root cause: `ir_is_cdata_function` omitted the `CCALL2FF` probe, so the
  recorder used the clobbered CALL result slot instead of the cdata function.
  The 2FF probe also still used stale cdata flags.
- `hypotf(3.0, 4.0)` now compiles and executes natively.
- FADDI arguments (`hypotf(v+1, v+2)`) also install after accepting
  IR-proven float slots.
- `tests/ffi/63_trace_ccall2ff.lua` and
  `tests/ffi/64_trace_faddi_2ff.lua` cover both.

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
