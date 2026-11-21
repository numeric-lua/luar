# Luar 0.93 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds table-valued table-field writes.

## Highlights

- `{ x = other_table }` lowers to `TSETKT`; the helper copies the table
  reference, applies `luaH_set`, and reloads any relocated base.
- `tests/jit/96_trace_table_field_value.lua` checks identity (`rawequal`),
  forced GC survival and the `alloc=0` constructor path.

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

- Struct/pointer cdata upvalues remain interpreter-side; only cdata arrays
  use `TAG_USERDATA`.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
