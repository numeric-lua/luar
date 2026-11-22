# Luar 0.135 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds table-valued writes on dynamic keys.

## Highlights

- `t[k] = inner` now uses `TSETDYN` with a table register value and
  preserves identity.
- Integer hash-insert/side-trace shapes are not intercepted: the dynamic
  path only selects float or table values.
- `tests/jit/121_trace_dynamic_key_table.lua` checks identity, forced GC and
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

- Other register value types on dynamic SETTABLE still fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
