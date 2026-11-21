# Luar 0.98 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes list-constructor element types in traces.

## Highlights

- `{ true, inner, "x", 1, 2.5 }` traces booleans, tables, strings, ints and
  floats.
- `LOADTRUE`/`LOADFALSE` lower to `KBOOL`; table elements use `TSETIT`.
- `tests/jit/99_trace_list_bool_table.lua` checks identity, forced GC and
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

- 64-bit cdata arithmetic remains interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
