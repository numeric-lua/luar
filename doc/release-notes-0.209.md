# Luar 0.209 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
widens the default J2 tier for cdata arrays and raw table reductions.

## Highlights

- Cdata arrays captured as closure upvalues now reduce natively
  (`s = s + a[i]`) and accept pure fills (`a[i] = v`, `a[i] = i`, numeric
  constants).  Both forms re-resolve the upvalue on every entry, so
  rebinding it to another array is observed exactly like the interpreter.
- 8-byte integer cdata destinations (`int64_t`/`uint64_t`) accept
  element-wise copies, source-scalar arithmetic and two-source arithmetic.
  The helper only writes the destination, applying the same
  `ffi_convert` C-store conversion as the interpreter, so no cdata
  temporary has to be materialised for an element read.
- Integer cdata arrays update in place with the full bitwise family
  (`&`, `|`, `~`, `<<`, `>>`, immediate counts and unary `~`/`-`).  Lua's
  integer conversion, two's-complement wrap and logical shift rules are
  preserved; a non-integral operand declines before the first store so the
  interpreter raises the exact error.
- A raw table reduction can read through a constant table field
  (`s = s + obj.arr[i]`).  The helper re-reads the field on every entry,
  keeps the GETFIELD/GETTABLE temporary, and falls back when the field is
  missing, replaced, or no longer a raw array.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make test-shared
make test-dist
make dynasm-check
tools/upstream-check.sh
```

New tests:

- `tests/ffi/144_j2_cdata_upval_sum.lua`
- `tests/ffi/145_j2_cdata_i64_store.lua`
- `tests/ffi/146_j2_cdata_bitwise_update.lua`
- `tests/ffi/147_j2_cdata_upval_fill.lua`
- `tests/jit/176_field_table_sum.lua`

## Known limitations

- Upvalue cdata support covers pure fills and single-array reductions;
  copy/update shapes that load the array twice remain interpreter-side.
- 8-byte integer elements are admitted only as store destinations.  A
  read would produce cdata int64 rather than a Lua number, so copy/update
  sources still require 1/2/4-byte integers or 4/8-byte floats.
- Bitwise/`unary` updates are limited to integer element views; float
  arrays stay interpreted because a per-element integer-conversion check
  would be required before the first store.
- The field-array reduction accepts a register table object and one
  constant string field.  General `__index`/`__newindex`, dynamic keys and
  chained field reads remain on the interpreter or the opt-in J4 tier.
