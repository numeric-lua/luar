# Luar 0.223 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends table and cdata store coverage to closure-upvalue destinations.

## Highlights

- Table arithmetic stores can write through a closure-upvalue destination:
  `U[i] = src[i] OP k`, `U[i] = a[i] OP b[i]`, and unary
  `U[i] = -src[i]` / `~src[i]`.
- Bounded-cdata stores can write through a closure-upvalue destination for
  copies, scalar/bitwise stores, two-source arithmetic/bitwise stores and
  unary minus/bitwise-not.
- The destination slot is refreshed by the first interpreted iteration and
  re-checked by each helper entry, so rebinding the upvalue is observed and
  a non-table/non-cdata destination falls back to the interpreter.

## Fixes

- J4 pointer-field element writes (`CPSETI`, `CPSETF`, `CPSETI64`) now
  dispatch to the matching `cset_ptr_*` helper instead of falling through
  the `CCALL0D` case.  The visible result was already correct because the
  bad call exited to the interpreter; the fused write now executes.

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

- `tests/jit/217_table_upval_arith_store.lua`
- `tests/ffi/189_cdata_upval_dest_store.lua`
- `tests/ffi/190_j4_ptr_field_store.lua`

## Known limitations

- The destination-prefix matcher handles one GETUPVAL destination; a source
  upvalue may also be present.
- Pointer/8-byte cdata reads and non-numeric table values remain
  interpreter-side.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
