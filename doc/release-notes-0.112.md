# Luar 0.112 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds `(int, double) -> double` native C calls.

## Highlights

- New `CCALL2ID` trace op and libffi helper.
- `ir_is_cdata_function` now recognizes the signature; previously the
  function upvalue was not tracked, so the CALL slot was not cdata.
- `tests/ffi/43_trace_ccall2id.lua` covers constant and MOVE arguments;
  both compile and execute.

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

- FADDI variable-source calls still fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
