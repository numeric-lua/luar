# Luar 0.217 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the cdef constant-expression parser, adds subtract dot-product and
raw-table scaled-store J2 intrinsics, and fixes two J4 miscompiles.

## Highlights

- `ffi.cdef` accepts dotted and indexed member designators in `offsetof`,
  so headers and static assertions can use `offsetof(S, i.b)` and
  `offsetof(S, a[1].x)`.  The final member may not be a bitfield.
- `sizeof("literal")` is evaluated in constant expressions with C string
  size, including decoded escapes and the trailing NUL.
- J2 compiles `s = s - a[i] * b[i]` and `s = s - a[i] / b[i]` over raw
  tables and bounded cdata arrays, completing the two-array reduction set.
- J2 compiles `dst[i] = src[i] +|-|*|/ k` over raw table arrays, with
  constant, register and upvalue scalars, `ADDI` immediates, in-place
  stores, destination growth and descending/strided runs.  The element and
  result registers keep their last bytecode values.

## Correctness

- J4 constant-string-key stores now check the target metatable for
  `__newindex` when the field is absent; they exit to the interpreter
  instead of writing the raw field.  A field that already exists still
  bypasses the handler, matching Lua.
- J4's `ADDI`/`FADDI` selection is guarded by the operand subtype.  The
  same trace can no longer return a float for an integer `+ 1` or treat a
  double payload as an integer when re-entered with the other flavour.
- J4 rejects constant `RK(C)` values in `OP_SETTABLE`; a hot `t[i] = nil`
  deletes the key instead of treating the constant-table index as a slot.
- J4's integer analyzer tracks table/string/boolean field reads and
  rejects table, string, userdata and boolean arithmetic operands, so
  `__add`/`__sub` handlers are called instead of a raw integer add.
- The raw-table scaled store grows the destination array to the maximum
  key in the run, fixing a descending-step out-of-bounds write.
- Metatables on store destinations, non-numeric source elements and
  unsupported shapes remain on the interpreter.

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

- `tests/ffi/174_nested_offsetof.lua`
- `tests/ffi/175_sizeof_string.lua`
- `tests/ffi/176_j2_cdata_pair_sub.lua`
- `tests/jit/193_pair_sub.lua`
- `tests/jit/194_table_scaled_store.lua`
- `tests/jit/195_j4_setfield_newindex.lua`
- `tests/jit/196_j4_addi_guard.lua`
- `tests/jit/197_j4_const_store.lua`
- `tests/jit/198_j4_arith_meta.lua`

## Known limitations

- `offsetof` designators support `.field` and `[index]`; pointer
  dereference forms are not part of C's member-designator grammar and are
  rejected.
- `sizeof` accepts a type name or one string literal, not arbitrary C
  expressions.
- J4 remains opt-in (`ir=1`) and roots only integer-controlled `for`
  loops; unsupported shapes fall back to J2 or the interpreter.
