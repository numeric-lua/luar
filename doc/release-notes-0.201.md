# Luar 0.201 release notes

This release closes two JIT miscompilations around loop-carried types and
float limits, adds a three-array sum intrinsic, and completes the
LuaJIT-compatible FFI metatype surface.

## JIT

* J2 now rejects any non-specialised loop whose carried registers change
  type between iterations.  The type-stability check previously ran only
  for branch bodies, so a straight-line body such as
  `t = u + i * 1.5; u = s - i` compiled with `u` as an integer and then read
  the double payload on the next native iteration.  The loop stays in the
  interpreter instead of producing a different result.
* The fast `while` peephole only accepts an integer limit.  It used to
  compare the raw 64-bit pattern of a double limit, so a subnormal limit
  whose bits were 100 ran 200 iterations instead of 101 and `100.0` could
  hang.  Float limits fall back to the generic mixed int/float compare.
* A new `s = s + a[i] + b[i] + c[i]` intrinsic covers three raw array reads
  in one loop.  It validates the three array shapes, metatables and element
  tags per entry and falls back without writing a partial accumulator.
* J4's float pass now verifies its result: a trace that still contains an
  integer arithmetic op on a float slot is not emitted.  Previously only
  the chain rooted at the first float slot was converted, so an independent
  `t = t * 1` or `u = s * -1` kept the integer opcode and reinterpreted the
  double payload.

## FFI

* Struct/union metatypes dispatch `__add`, `__sub`, `__mul`, `__div`,
  `__idiv`, `__mod`, `__pow`, `__band`, `__bor`, `__bxor`, `__shl`,
  `__shr`, `__unm`, `__bnot` and `__concat`, with Lua's left-then-right
  handler selection.  Numeric cdata keeps its FFI arithmetic.
* A metatype `__gc` is the default finalizer; `ffi.gc` overrides it and
  `ffi.gc(x, nil)` suppresses it.
* `__tostring`, `__len` and `__pairs` now resolve through a pointer to the
  struct/union type, completing the pointer forwarding already provided for
  `__index`, `__newindex` and `__call`.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tests/upstream/run.sh` and
`tools/upstream-check.sh` are green.  New regression files:
`tests/jit/160_straight_type_change.lua`,
`tests/jit/161_three_table_sum.lua`,
`tests/jit/162_while_float_limit.lua`,
`tests/jit/163_ir_mixed_float.lua`,
`tests/ffi/114_metatype_arith.lua`,
`tests/ffi/115_metatype_gc.lua` and
`tests/ffi/116_metatype_pointer.lua`.
