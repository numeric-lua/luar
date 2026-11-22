# Luar 0.122 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes table-upvalue field writes for tables.

## Highlights

- `t.x = inner` where `t` and `inner` are closed-over tables traces and
  preserves identity.
- `TSETUPKT` reads the value from its stack slot and applies `luaH_set` on
  the current closure’s upvalue table.
- `tests/jit/113_trace_settabup_table_field.lua` checks identity, forced GC
  and `alloc=0`.

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

- Register values other than tables remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
