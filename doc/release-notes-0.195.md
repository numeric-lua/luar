# Luar 0.195 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
expands J2's numeric and table coverage, makes the opt-in raw table emitter
run on Lua 5.5's split array representation, and fixes FFI `_Bool`
conversion.

## Highlights

- J2 compiles integer floor division and modulo (`//`, `%`, `IDIVK`,
  `MODK`).  The generated code adjusts x86-64's truncating `IDIV` to Lua's
  floor rule, and exits to the interpreter for zero divisors and the
  `-1`/`INT64_MIN` special cases so errors and wrapping are unchanged.
- J2 compiles constant-left shifts (`OP_SHLI`), covering `3 << i` with
  negative counts and counts of 64 or more.
- J2 compiles numeric `for` loops with float control values.  The native
  epilogue follows the interpreter's limit/step/index layout, IEEE addition
  and NaN-aware `floatforloop` termination.
- The opt-in raw table emitter now uses Lua 5.5's split array
  representation (an inverted `Value` array plus a separate tag array) and
  the collectable table tag.  Previously the entry guard could never match;
  once matched, the old 16-byte access would have written over neighbouring
  array slots.  The fast `s = s + t[i]` path also advances the index and
  materialises the `GETTABLE` result register.
- FFI `_Bool` conversion follows C: every non-zero scalar, including
  fractional, infinite and NaN floats, becomes 1; `_Bool` bitfields store
  and read the canonical 0/1 form.  Truncating to an integer first used to
  turn `0.5` into 0.
- `luar.version_num` now reports 19500; the numeric minor component had
  lagged behind the version string.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make dynasm-check
tools/upstream-check.sh
```

New tests:

- `tests/jit/136_generic_table_array.lua`
- `tests/jit/137_fast_table_sum.lua`
- `tests/jit/138_divmod.lua`
- `tests/jit/139_shli.lua`
- `tests/jit/140_float_for.lua`
- `tests/ffi/104_bool_conversions.lua`

## Known limitations

- J2 remains a loop-level compiler.  Calls, generic `__index`/`__newindex`,
  aggregate values and non-whitelisted opcodes stay on the interpreter or
  the opt-in J4 tier.
- The raw table emitter is still behind `jit.opt.start("table=1")`; it
  handles raw integer array reads/stores and float stores without
  metamethod dispatch.  Indexing outside the array part exits.
- Float `for` loops are native only when the body is in the numeric
  whitelist; a float `//`/`%` or a bitwise operation falls back at that
  instruction.
- The FFI `_Bool` fix applies to the scalar and bitfield conversion paths;
  arbitrary pointer/integer casts remain the caller's responsibility.
