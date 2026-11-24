# Luar 0.219 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends raw-table element stores, adds nested runtime `ffi.offsetof`, and
fixes two J4 store precision/key bugs.

## Highlights

- Raw table unary stores `dst[i] = -src[i]` and `dst[i] = ~src[i]` run
  natively.  Unary minus wraps integers or flips the float sign; bitwise
  not requires integer elements.
- Two-source table stores `dst[i] = a[i] OP b[i]` cover
  add/subtract/multiply/divide/floor-divide/modulo, in-place destinations
  and the compiler's separate element/result temps.
- Bitwise stores cover `dst[i] = src[i] &|~|<<|>> k` and
  `dst[i] = a[i] OP b[i]`.  Constant, register, immediate and upvalue
  scalars are supported; negative shifts reverse direction, 64 or more
  yields zero, and float operands remain interpreted.
- The runtime `ffi.offsetof` accepts dotted/indexed member designators
  (`i.b`, `a[1].x`), matching the cdef constant-expression parser, while
  direct fields and bitfield byte/bit/width results are unchanged.

## Correctness

- J4's generic TSET path now accepts only integer keys.  A string, float
  or boolean key with a metatable falls back to the interpreter instead of
  using its raw payload as an integer key.
- J4's constant integer `SETFIELD` (TSETKK) carries the full 64-bit value
  as raw bits instead of a double, so values above 2^53 no longer round.
- Store helpers validate source shapes and integer element types before
  writing; metatables, float bitwise operands, non-numeric values and
  integer zero divisors stay on the interpreter.

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

- `tests/ffi/179_runtime_offsetof_path.lua`
- `tests/jit/203_table_unary_store.lua`
- `tests/jit/204_table_pair_store.lua`
- `tests/jit/205_table_bitwise_store.lua`
- `tests/jit/206_table_pair_bitwise.lua`
- `tests/jit/207_j4_string_key.lua`
- `tests/jit/208_j4_tsetkk_i64.lua`

## Known limitations

- Bitwise element stores require integer tags; exact-integral float
  elements fall back to the interpreter.
- Raw-table store/copy helpers cap destination growth at `1 << 22` keys.
- `ffi.offsetof` designators support `.field` and `[index]`; pointer
  dereference forms are not part of the member-designator grammar.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
