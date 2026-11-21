# Luar 0.79 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant-string table field writes.

## Highlights

- `{ x = "hello" }` lowers `SETFIELD` with the `k` flag to `TSETKKS`; the
  interned key and string value come from the Proto constant table.
- The string is rooted by the Proto; `luaH_set` applies the field and the
  emitter reloads the relocated base.
- `tests/jit/84_trace_constant_string_field.lua` covers tracing, GC and
  `alloc=0`.

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

- Dynamic string values and float/string field reads remain
  interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
