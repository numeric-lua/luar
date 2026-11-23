# Luar 0.208 release notes

This release hardens J4 side-trace validation, fixes two recorder
initialization bugs, validates zero-length `ffi.copy` sources, and extends
the default J2 cdata write tier with floor division and modulo.

## JIT

* J4 side traces now always run the float/integer analysis, even when the
  root trace was integer-only.  A side body using a float-constant
  `ADDK`, `SUBK`, `MULK`, `MODK` or `IDIVK` without a float guard could
  previously be emitted with the integer constant 0 and retagged as an
  integer, producing a wrong value on the final loop iteration.
  Unsupported float K ops fall back to the interpreter; `MULK` and `DIVK`
  still specialize to `FMULK`/`FDIVK`, and the IR verifier rejects the
  unsupported forms before emission.
* The IR recorder initializes its per-instruction emit flag and marks the
  pending int64 and field-chain paths explicitly.  This removes the
  uninitialized read in the nested cdata field-chain scanner and keeps the
  int64 array fusion paths deterministic.
* J2 stores floor division and modulo over bounded cdata arrays:
  `a[i] = a[i] // v`, `a[i] = a[i] % v`, `a[i] = b[i] // v`,
  `a[i] = b[i] % v`, `a[i] = b[i] // c[i]` and `a[i] = b[i] % c[i]`.
  Register, immediate and constant scalar operands are supported for the
  two-array forms, along with strided and descending loops, small integer
  destinations and 4/8-byte float elements.  The helpers reproduce Lua's
  floor/negative-operand and int-wrap rules, keep the bytecode-visible
  source/result temporaries, and decline an integer zero divisor before
  any store so the interpreter raises the exact `n//0`/`n%0` error.  Float
  zero follows the non-raising inf/NaN path.

## FFI

* `ffi.copy` validates its source even for a zero-length copy.  Numbers,
  tables, booleans and scalar cdata were previously accepted silently
  because the function returned before the source check.  Only a
  pointer/array cdata or `nil` as a zero-length NULL source is accepted
  now; the existing string, overlap (`memmove`) and bounds behaviour is
  unchanged.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tools/upstream-check.sh` and
`make dynasm-check` are green.  New coverage:

* `tests/jit/176_j4_side_float_k.lua`
* `tests/ffi/140_copy_source_validation.lua`
* `tests/ffi/141_j2_cdata_pair_divmod.lua`
* `tests/ffi/142_j2_cdata_divmod_store.lua`
* `tests/ffi/143_j4_field_chain.lua`
