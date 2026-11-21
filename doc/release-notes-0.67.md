# Luar 0.67 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant-string table fields to J4 traces.

## Highlights

- **`TGETK`/`TSETK`**: `t.x` and `t.x = i` lower with the interned string
  key from the Proto constant table.
- Works for `{x = i}` constructors and empty-table field writes.
- Writes copy the value and return the relocated base after a possible
  table growth; reads use `luaH_getstr` and do not allocate.
- `tests/jit/78_trace_fields.lua` covers both forms, GC and `alloc=0`.

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

- Field values are integers; string/float/table values fall back.
- Dynamic string keys and `SETTABLE`-with-string keys remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
