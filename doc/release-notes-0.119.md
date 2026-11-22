# Luar 0.119 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant 64-bit integer writes through table upvalues.

## Highlights

- `t.x = 1099511627777` where `t` is a closed-over table traces and stores
  the full 64-bit value (bits carried in `nval`, not truncated through a
  double).
- `tests/jit/110_trace_settabup_int_field.lua` writes and reads the value,
  with forced GC and `alloc=0`.

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

- Upvalue field writes accept constant int/float only.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
