# Luar 0.68 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
closes the top-level C-function upvalue gap.

## Highlights

- **`GETUPVAL`**: a loop closing over `local abs = ffi.C.abs` now traces.
  `LUAR_IR_ULOAD` stores the expected `LClosure` pointer and upvalue index;
  the helper verifies closure identity before copying the upvalue value.
- Native one-argument C calls (`abs`) work whether the function is a local
  register or a closure upvalue.
- `tests/ffi/35_trace_upvalue_ccall.lua` covers the upvalue form, negative
  arguments and closure identity changes.

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

- Upvalue loading is guarded per closure; if the trace's Proto is reused
  with a different closure, the guard exits to the interpreter.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
