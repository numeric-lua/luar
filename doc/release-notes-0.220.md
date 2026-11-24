# Luar 0.220 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends bitwise and unary cdata stores and fixes two J4 table-read misses.

## Highlights

- Two-source cdata stores now cover `a[i] = b[i] OP c[i]` for `&`, `|`,
  `~`, `<<` and `>>` as well as the arithmetic family, including in-place
  destinations.  Negative shift counts reverse direction and 64 or more
  yields zero.
- Scaled cdata stores cover `a[i] = b[i] &|~|<<|>> k` with constant,
  immediate and register scalars.
- Cdata unary stores cover `a[i] = -b[i]` and `a[i] = ~b[i]`, preserving
  separate element/result temporaries and supporting in-place, descending
  and strided ranges.

## Correctness

- J4 integer-key reads initialize the result before `luaH_getint`.  A
  missing key now exits to `__index`; previously a later miss in the same
  trace reused the previous hit's element.
- J4 string-key reads and the float/table/bool/string field helpers do the
  same before `luaH_getstr`, fixing the stale-value miss for `t.b` after
  `t.a` (and the typed variants).
- Bitwise stores require integer elements; float arrays remain
  interpreted, and shifts guard `INT64_MIN` counts.

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

- `tests/ffi/180_j2_cdata_bitwise_pair.lua`
- `tests/ffi/181_j2_cdata_bitwise_scalar.lua`
- `tests/ffi/182_j2_cdata_unary_store.lua`
- `tests/jit/209_j4_tgeti_miss.lua`
- `tests/jit/210_j4_tgetk_miss.lua`

## Known limitations

- Cdata bitwise stores require integer element arrays and integer scalars;
  exact-integral float values stay interpreted.
- Cdata scalar stores do not yet accept an upvalue scalar in the five-slot
  shape; upvalue sources that include a `GETUPVAL` instruction remain
  interpreted.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
