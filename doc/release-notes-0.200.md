# Luar 0.200 release notes

This release fixes two JIT correctness/performance gaps, widens raw-table
sum coverage, and aligns two FFI numeric conversions with LuaJIT.

## JIT

* The fast `while`/`repeat` peephole now keeps exactly one accumulator.
  A body such as `a = a + i*i; a = a*i` matched both the square and the
  product shapes; the emitter chose one and silently computed a different
  loop.  The same class covered `a = a + i; a = a*i` and
  `a = a - i; a = a*i`.  These bodies now fall back to the generic while
  decoder (or the interpreter) and produce the interpreter's result.
* `s = s + t[i]` with `t` captured as a closure upvalue is compiled to the
  raw-array sum intrinsic.  The upvalue is resolved on every entry, so
  reassigning it, growing the array, or adding a metatable still falls back
  correctly.  A 3M-element sum runs about 11x faster than the interpreter
  on the development host.
* Raw array and keyed sum intrinsics no longer refuse a remaining count
  above `1 << 22`.  A sum performs no allocation and is already bounded by
  the table's 32-bit array size; the old cap made every backedge of a 5M
  loop attempt and reject the intrinsic, which was slower than not
  compiling it at all.
* A raw-table intrinsic that cannot handle the current iteration is now
  dropped at that backedge instead of being retried forever.  A missing key
  late in a large keyed sum previously caused quadratic re-scanning.

## FFI

* Enum cdata converts numerically as an unsigned 32-bit value, matching
  LuaJIT.  `ffi.new("enum E", -1)` still displays as `-1`, but `tonumber`,
  casts, arithmetic and comparisons use 4294967295; `e == 0xffffffff` is
  true and `e == -1` is false.  Struct enum fields use the same view.
* Integer cdata power returns the `INT64_MAX` infinity sentinel for a zero
  base with a negative exponent.  It previously returned zero, while
  LuaJIT's observable result is `9223372036854775807LL`.  Unsigned cdata,
  `+/-1` bases and positive exponents are unchanged.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test` and
`make WITH_FFI=1 test-asan` are green.  The new regression files are
`tests/jit/157_upvalue_table_sum.lua`,
`tests/jit/158_while_multi_acc.lua`,
`tests/jit/159_table_sum_large.lua`,
`tests/ffi/112_enum_unsigned.lua` and
`tests/ffi/113_pow_zero_negative.lua`.

The FFI arithmetic/comparison differential corpus (99,552 expressions over
all integer widths, enums, floats and Lua numbers) now matches LuaJIT with
zero differences.
