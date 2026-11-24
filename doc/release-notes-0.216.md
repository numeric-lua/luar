# Luar 0.216 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
broadens the default J2 reduction set with weighted division and subtraction
and with two-array division, and fixes a keyed-sum regression introduced by
the raw-table affine fill.

## Highlights

- J2 compiles `s = s + a[i] * k` and `s = s + a[i] / k`, including the
  subtract spelling `s = s - ...`, over raw Lua tables and bounded cdata
  arrays.  The scalar can be a numeric constant, a register local or a
  closure upvalue; the upvalue is re-resolved on every entry.
- Division always yields a float, even for two integers.  A zero divisor
  follows IEEE (`inf`/`NaN`) instead of raising, matching the interpreter.
  Integer multiplication keeps two's-complement wrap; a float operand
  promotes the whole chain.
- J2 compiles `s = s + a[i] / b[i]` over two raw tables or two bounded cdata
  arrays.  The division is evaluated left-to-right; `b[i] / a[i]` is a
  different reduction and follows the same read order.
- The element and product/quotient registers keep the values the bytecode
  would have after the loops, so code that reads those locals afterwards
  sees the interpreter's result.

## Correctness

- The affine fill intrinsic (`t[i] = i + k`, `t[i] = i * k`,
  `t[i] = k`) reused the keyed-sum intrinsic kind.  A hot keyed sum then
  ran the fill helper, stopped after the interpreted prefix and left the
  accumulator unchanged (`tests/jit/24`, `151` and `159`).  The fill has a
  dedicated kind and unknown kinds now fail closed.
- `luar.version_num` now encodes 0.216.0; the minor component had lagged
  behind the release string.
- Untested or unsafe shapes stay on the interpreter: pointer sources
  (which have no bounded length), out-of-bounds runs, non-numeric elements
  and scalars, and a destination that would overwrite the source table or
  array register.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
tools/upstream-check.sh
```

New tests:

- `tests/jit/188_scaled_div.lua`
- `tests/jit/189_tint_kind_collision.lua`
- `tests/jit/190_scaled_sub.lua`
- `tests/jit/191_pair_div.lua`
- `tests/ffi/171_j2_cdata_scaled.lua`
- `tests/ffi/172_j2_cdata_scaled_sub.lua`
- `tests/ffi/173_j2_cdata_pair_div.lua`

## Known limitations

- These are J2 loop-level intrinsics, not general traced expressions; the
  loop body must still match the exact bytecode shape.
- J4 remains opt-in (`jit.opt.start("ir=1")`) and roots only
  integer-controlled `for` loops; other backedges fall back to J2 or the
  interpreter.
- `ffi.cdef` still does not parse arbitrary system headers, and 64-bit
  cdata element reads remain interpreter-side because they produce cdata
  rather than Lua numbers.
