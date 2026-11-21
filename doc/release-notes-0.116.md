# Luar 0.116 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes field-read value types with tables.

## Highlights

- `t.x` with a table field lowers to `TGETKT`; the helper copies the
  rooted table reference.
- `tests/jit/107_trace_table_field_read.lua` checks identity, forced GC and
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

- Table field reads require a local table with a live field at record.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
