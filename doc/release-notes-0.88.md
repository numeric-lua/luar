# Luar 0.88 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds table length to J4 traces.

## Highlights

- `#t` lowers to `TLEN` when the operand is a table; `luaH_getn` returns the
  boundary directly with no allocation.
- Tables with a `__len` metamethod stay interpreter-side: the helper
  refuses to compute a length and the trace exits.
- `tests/jit/92_trace_table_len.lua` covers array tables, `__len` fallback
  and the `alloc=0` constructor path.

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

- Tables whose boundary requires a metamethod or slow hash scan after
  mutation still fall back if the helper rejects.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
