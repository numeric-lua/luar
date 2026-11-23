# Luar 0.199 release notes

This release fixes four JIT correctness/performance gaps and aligns FFI
integer arguments with LuaJIT's C conversion rules.

## JIT

* Branch-loop entry guards now use the operand flavour for float
  `ADDK`/`SUBK`/`MULK`/`DIVK` and `UNM`.  These loops compiled but always
  failed their own integer guard, so `if cond then s = s + 0.5 end` and
  float field/constant branches silently stayed interpreted.
* A branch body may now contain the loop's one raw array read
  (`GETI`/`GETTABLE`), not only a read at the first instruction.  The table,
  metatable, bounds and element-tag guards are unchanged, and a failed guard
  resumes at the original bytecode PC.
* Inlined leaf calls bail out to the interpreter on a zero divisor instead
  of turning `//` or `%` by zero into a zero result.  A loop that reaches the
  divisor only after the native run starts now raises exactly as T0 does.
* Integer-accumulator raw table and keyed sums promote to float when the
  first float element/value is added, matching the interpreter's mixed
  arithmetic instead of falling back.  `jit.stats().tint_hits` reports
  successful intrinsic runs.

## FFI

* FFI integer arguments use C conversion rules: finite Lua floats truncate
  towards zero and numeric strings are rejected.  This covers `ffi.sizeof`
  VLAs, `ffi.new` VLAs, `ffi.string`, `ffi.copy`, `ffi.fill`, the `ffi.fill`
  byte, `ffi.errno` and cdata array indices.  NaN, infinity and out-of-range
  values raise instead of invoking an undefined C conversion.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make dynasm-check` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/jit/149_float_branch_guards.lua`,
`tests/jit/150_leaf_divzero.lua`,
`tests/jit/151_table_float_promote.lua`,
`tests/jit/152_branch_table_read.lua` and
`tests/ffi/111_arg_coercion.lua`.
