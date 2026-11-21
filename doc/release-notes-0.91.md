# Luar 0.91 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds dynamic string table-field writes.

## Highlights

- `{ x = x }` with a string value lowers to `TSETKS`; the value is copied,
  `luaH_set` applies it, and the emitter reloads the relocated base.
- `tests/jit/95_trace_dynamic_string_field.lua` covers tracing, forced GC
  and the `alloc=0` constructor path.

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

- Dynamic table values remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
