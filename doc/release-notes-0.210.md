# Luar 0.210 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the default J2 cdata tier, closes FFI compatibility gaps and fixes
several J4 trace-result/call-width miscompiles.

## Highlights

- J2 updates bounded cdata arrays held in closure upvalues in place
  (`a[i] = a[i] +|-|*|/|//|% v`, the bitwise family, shifts and unary
  `~`/`-`).  The upvalue is re-resolved on every native entry; rebinding it
  to another array is observed like the interpreter, and a non-array binding
  falls back to the interpreter so its original error is raised.
- J2 reduces and updates enum-element cdata arrays.  Elements use the same
  unsigned 32-bit numeric view as `ffi` field/array reads, so high-bit
  values and negative enum constants behave exactly like the interpreter.
- J2 stores into store-only `_Bool` cdata arrays.  Pure fills, value stores,
  copies, source-scalar stores and two-source stores all apply C `_Bool`
  normalization (any non-zero scalar becomes 1), never load a bool element
  into a numeric slot, and keep the bytecode-visible temporary.
- A metatype `__new` handler now overrides construction when the CType
  object itself is called (`ct(...)`), matching LuaJIT.  `ffi.new` and
  `ffi.cast` keep the built-in constructor and do not re-enter the handler.
- `ffi.new` accepts a Lua string only for a 1-byte character pointer
  (`char *` and its signed/unsigned/const spellings).  A `void *` or other
  pointer type now rejects the initializer instead of silently reinterpreting
  the bytes; `ffi.cast` and string arguments in calls stay permissive.

## Correctness fixes

- J4 pointer-field reads/stores accept only the widths the helpers address
  (4/8-byte integers and 8-byte doubles).  `char *`, `short *` and `float *`
  fields used the wrong stride and could return wrong values or overwrite
  neighbouring elements; those loops now stay interpreted.
- J4 scalar C-call results keep the interpreter's type and width.  64-bit
  integer results remain cdata (the loop is interpreted), narrow integers
  are extended by declared signedness, and `float`-returning zero-argument
  calls are read as 4-byte floats rather than doubles.
- CType objects no longer compare equal to an ordinary cdata value of the
  denoted type; CType-to-CType equality is unchanged.
- The arm64 non-HFA large-union by-value argument override is no longer
  cleared, restoring AAPCS64 by-reference passing.
- `jit.opt.start("hotloop=0")` disables tracing for new Protos instead of
  silently retaining the default threshold.
- J4 slot-writer analysis includes every result-writing IR op, so a stale
  constant cannot be substituted for a later table or call result.

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

- `tests/ffi/148_metatype_new.lua`
- `tests/ffi/149_new_pointer_string.lua`
- `tests/ffi/150_j2_enum_array.lua`
- `tests/ffi/151_j2_bool_fill.lua`
- `tests/ffi/152_j2_cdata_upval_update.lua`
- `tests/ffi/153_j4_call_widths.lua`
- `tests/ffi/154_j4_ptr_field_widths.lua`
- `tests/ffi/155_ctype_equality.lua`
- `tests/jit/177_hotloop_zero.lua`

## Known limitations

- Upvalue cdata support now covers reductions, pure fills and in-place
  updates.  Copy forms that reload two different upvalues per iteration
  remain interpreter-side.
- 8-byte integer elements are still admitted only as store destinations.
  A read yields cdata int64, not a Lua number, so copy/update sources keep
  requiring 1/2/4-byte integers or 4/8-byte floats.
- `_Bool` arrays are store-only in the default tier.  A read yields a Lua
  boolean, so reductions and arithmetic over bool arrays stay interpreted.
- J4 pointer-field elements are limited to 4/8-byte integers and 8-byte
  floats; narrower/float32 fields stay interpreted until width-aware
  helpers exist.  C calls returning 64-bit integers or aggregates also
  stay interpreted rather than exposing a cdata result to the trace.
- `__new` dispatch applies to CType-object calls only; there is no
  equivalent hook on `ffi.new`.
