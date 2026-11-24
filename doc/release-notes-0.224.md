# Luar 0.224 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends table copy and fill coverage to arbitrary values.

## Highlights

- Table element copies `dst[i] = src[i]` now handle strings, tables,
  booleans and nil holes as well as numbers.  Register and closure-upvalue
  destinations and sources are supported; numeric runs keep the fast path,
  while non-numeric or sparse runs use `luaH_getint`/`luaH_setint`.
- Table fills accept non-numeric constant, register and upvalue values:
  `t[i] = "x"`, `t[i] = true`, `t[i] = obj`, `t[i] = U`.  A closure-upvalue
  destination (`U[i] = value`) is also supported.
- Numeric copies and fills keep their existing specialised paths.

## Correctness notes

- Generic fills and copies check the destination metatable and decline so
  `__newindex` is not bypassed.
- `luaH_setint` supplies the GC write barrier for collectable values and
  removes an entry for nil.
- The source/destination run is capped at `1 << 22` keys and requires
  positive keys for the generic path.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make test-shared
make test-dist
tools/upstream-check.sh
```

New tests:

- `tests/jit/218_table_value_copy.lua`
- `tests/jit/219_table_value_fill.lua`

## Known limitations

- Generic copies use per-element hash calls and are slower than the numeric
  fast path.
- Negative or zero keys on the generic path remain interpreter-side.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
