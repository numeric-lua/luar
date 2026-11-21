# Luar 0.115 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes field-read value types with booleans.

## Highlights

- `t.x` with a boolean field lowers to `TGETKB`.
- `tests/jit/106_trace_boolean_field_read.lua` covers local constructor
  tables, forced GC and `alloc=0`.
- Table upvalues with stale slots still fall back.

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

- Boolean field reads require a local table with a live field at record.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
