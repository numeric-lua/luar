# Luar 0.100 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a narrow dynamic-key table-write path.

## Highlights

- `t[k] = v` traces when `k` comes from an upvalue load and the table has no
  metatable.
- The path is deliberately narrower than generic `SETTABLE`: it does not
  intercept integer hash inserts, constant-key writes, `__newindex` tables,
  or side-trace shapes.
- `TSETDYN` validates the key/value tags at runtime and returns the
  relocated base.
- `tests/jit/100_trace_dynamic_table_set.lua` covers tracing, forced GC and
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

- Dynamic keys are string-only and upvalue-loaded; other shapes fall back.
- 64-bit cdata arithmetic remains interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
