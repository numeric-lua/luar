# Luar 0.76 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds dynamic float table-field writes.

## Highlights

- `{ x = x }` with a float value lowers to `TSETKF`; the value is copied,
  `luaH_set` applies it, and the emitter reloads the relocated base.
- `ir_floatify` now treats a float tag guard as float evidence, fixing
  traces whose only float producer is a parameter guard.
- `tests/jit/82_trace_float_field.lua` covers tracing, GC and `alloc=0`.

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

- Float field reads (`TGETKF`) and dynamic string/table values remain.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
