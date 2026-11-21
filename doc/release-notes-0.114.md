# Luar 0.114 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes dynamic field-write value types with booleans.

## Highlights

- `{ x = b }` with a boolean value lowers to `TSETKDB`; the helper validates
  the value tag and applies `luaH_set`.
- `tests/jit/105_trace_dynamic_boolean_field.lua` checks true/false, forced
  GC and `alloc=0`.

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

- FADDI variable-source calls remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
