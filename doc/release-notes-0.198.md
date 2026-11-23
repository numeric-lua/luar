# Luar 0.198 release notes

This release fixes two observable J2/FFI correctness bugs and makes
float-controlled `for` loops actually run natively, including loop bodies
with branches.

## JIT

* Float-controlled `for` loops now execute their compiled code.  The VM
  recorded and compiled a float `FORLOOP` backedge but only called the
  recorder, never the generated function, so `jit.stats().compiled` grew
  while every iteration stayed on the interpreter.  The float path now
  hands the next iteration to J2 exactly like the integer path, with normal
  guard-exit and `jit.flush()` handling.
* Branches inside a float-controlled `for` loop compile again.  Branch
  loops install entry guards for the count/step/index triplet; those guards
  used the integer tag unconditionally, so any float loop containing an
  `if` failed its own entry check before running.
* A fused `acc = acc + t[const]` (`OP_GETI`) keeps the bytecode result
  register live.  The interpreter writes the element to `R[A]` before the
  add; the native fused path only updated the accumulator, so
  `local x = t[1]; s = s + x` left `x` at its pre-loop value after a native
  run.  The emitter now materialises `R[A]`, and the fusion is refused when
  the lookup destination is also the accumulator.
* Float `for` branch emission also fixes a DynASM if/else grouping bug: the
  `lclose` label was only defined on the non-branch path, so the first
  float branch loop failed `dasm_link`.

## FFI

* A metatype's `__eq` is dispatched when both operands are the same struct
  or union cdata, as in LuaJIT.  Stock Lua short-circuits identical
  userdata before looking up `__eq`.  The FFI-aware equality hook now calls
  the handler; without a handler, raw identity still returns true, and
  `rawequal()` remains raw.
* Pointer subtraction validates its base types.  Same-size integer types
  (including enums) are compatible; different struct tags, struct/union
  mixes, different widths and function pointers raise instead of silently
  dividing by an unrelated element size.  Floating Lua offsets truncate
  towards zero, and NaN, infinite or out-of-range offsets act as zero, all
  matching LuaJIT's observable results.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test` and
`make WITH_FFI=1 test-asan` are green.  The new regression tests are
`tests/jit/146_geti_result.lua`, `tests/jit/147_float_for_native.lua`,
`tests/jit/148_float_for_branch.lua`, `tests/ffi/109_pointer_compat.lua`
and `tests/ffi/110_metatype_eq_identity.lua`.
