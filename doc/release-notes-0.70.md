# Luar 0.70 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
allows closed-over tables to be trace targets.

## Highlights

- **Table upvalues**: `local t; function f() ... t[i] = i; ... t[i] ...`
  traces when the upvalue is a table; ULOAD's closure identity guard and
  table tag guard protect the path.
- **Tag analysis fix**: `TGET` results no longer inherit a stale table tag,
  which had blocked integer analysis for table-upvalue loops.
- `tests/jit/80_trace_upvalue_table.lua` covers fill/read, forced GC and two
  different table upvalues.

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

- The table must be an upvalue or local register; dynamic keys remain limited
  to the supported TSET/TGET forms.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
