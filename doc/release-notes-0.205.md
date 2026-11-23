# Luar 0.205 release notes

This release fixes two default-tier J2 miscompiles, restores full-width
integer constants in the opt-in J4 tier, closes FFI struct/union and
pointer-conversion gaps against LuaJIT, and extends the default J2 cdata
reductions to paired arrays.

## JIT

* J2 runs bounded two-source cdata reductions in the default tier:
  `s = s + a[i] + b[i]`, `s = s + a[i] - b[i]` and the dot-product /
  sum-of-squares forms `s = s + a[i] * b[i]` and `s = s + a[i] * a[i]`.
  The helper follows the interpreter's FORLOOP key sequence for any nonzero
  integer step, preserves int wrap and float promotion per operation,
  updates the visible element and product temporaries, and is observed
  through `jit.stats().tint_hits`.  Pointer sources and mixed table/cdata
  pairs stay on the interpreter.  This is coverage plus performance, not a
  new release gate.
* The raw table-fill intrinsic no longer discards existing hash entries
  when it grows the array part.  Filling a table that already had
  `t[1000]`, `t.key` or `t[-1]` used to lose those entries silently; the
  fill now keeps the hash part.
* Leaf-call intrinsics no longer snapshot loop-varying extra arguments.
  Shapes such as `s = s + f(i, i)` and `s = s + f(i, s)` used the first
  iteration's argument for the whole native run.  Those loops now stay on
  the interpreter; a genuinely loop-invariant argument still compiles.
* J4 integer `ADDK`/`SUBK`/`MULK` now emit full-width constants through the
  scratch-register helpers.  Constants such as `2^32`, `2^31` and `maxint`
  were previously truncated to their low 32 bits, giving wrong results in
  `ir=1` loops.

## FFI

* A union table initializer chooses the first named member that is present,
  as LuaJIT does.  Previously `ffi.new("union { int i; double d; }",
  { d = 1.5 })` ignored `d` and returned a zero union.  Positional entries
  still initialize only the first member.
* An aggregate lvalue can initialize a `void *` in `ffi.new` and pointer
  assignments, matching C and LuaJIT.  Mismatched object pointers and
  numeric scalars remain rejected.
* `ffi.metatype` enforces one metatable per struct/union: a second call
  raises even when it passes the original table again, instead of silently
  leaving the first table installed.
* `__len` and `__pairs` on a struct/union without a metatype now raise the
  intended cdata error instead of indexing a missing registry entry and
  reporting "attempt to index a nil value".

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tools/upstream-check.sh` and
`make dynasm-check` are green.  New coverage:

* `tests/ffi/125_union_named_init.lua`
* `tests/ffi/126_void_pointer_aggregate.lua`
* `tests/ffi/127_metatype_single_registration.lua`
* `tests/ffi/128_no_metatype_dispatch.lua`
* `tests/ffi/129_j2_cdata_pair.lua`
* `tests/jit/173_fill_preserves_hash.lua`
* `tests/jit/174_leaf_arg_variation.lua`
* `tests/jit/175_j4_int64_k_constants.lua`
