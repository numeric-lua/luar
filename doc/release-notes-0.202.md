# Luar 0.202 release notes

This release fixes a silent fast-while miscompilation, makes FFI integer
arguments and bitfield introspection match LuaJIT more closely, and adds
native two-array sum/difference and dot-product kernels.

## JIT

* The fast `while` peephole keeps one step for the induction variable.  A
  body that wrote it twice (`i = i + 1; i = i + 1`, or two different steps)
  used to overwrite the recorded step and run a different loop from the
  interpreter.  Such loops now fall back to the generic emitter or the
  interpreter.
* J2 sums and subtracts two raw arrays in one C pass:
  `s = s + a[i] + b[i]` and `s = s + a[i] - b[i]`.  Register locals,
  parameters and closure upvalues are supported.  The intrinsic validates
  both array shapes and every element tag before writing, and preserves the
  accumulator plus the element/temporary registers the bytecode can observe
  after the loop.
* J2 computes dot products and sums of squares:
  `s = s + a[i] * b[i]` and `s = s + a[i] * a[i]`.  It follows the
  interpreter's int64 wrap and mixed int/float promotion order, including a
  promotion in the middle of a run, and keeps the product temporary live for
  post-loop reads.
* `jit.stats().tint_hits` observes both new intrinsic families.

## FFI

* Integer and length arguments accept numeric cdata: VLA lengths,
  `ffi.sizeof`, `ffi.string`, `ffi.copy`, `ffi.fill`, `ffi.errno` and
  integer cdata array indices.  Float cdata is truncated for lengths but
  rejected as an array index, as in LuaJIT.
* `ffi.offsetof` now reports packed and full-width bitfields the way LuaJIT
  does: narrow packed fields are byte-relative, wider packed fields use the
  base type's storage unit, and a full-width field that starts on a storage
  boundary is returned like an ordinary member.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tests/upstream/run.sh` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/jit/164_fast_while_multi_inc.lua`,
`tests/jit/165_pair_array_sum.lua`,
`tests/jit/166_dot_product.lua`,
`tests/ffi/117_offsetof_bitfields.lua` and
`tests/ffi/118_cdata_int_args.lua`.
