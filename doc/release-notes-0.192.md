# Luar 0.192 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes two J2 numeric-loop miscompiles, extends J2 with integer bitwise and
constant-shift loops, and corrects two runtime/FFI edge cases.

## Highlights

- J2 now keeps full 64-bit integer constants in `ADDK`/`SUBK`/`MULK` and the
  inlined `__index` multiplier.  Constants above `INT_MAX` used to be
  truncated to a 32-bit immediate, so compiled loops disagreed with the
  interpreter.
- Mixed integer/float comparisons are exact at the `2^53` boundary.  A
  native loop that would round a large integer now exits at that comparison
  and lets the interpreter finish, without re-entering the loop forever.
- New J2 coverage: integer `&`, `|`, `~`, unary `~`, and constant `<<`/`>>`
  loop bodies, including the K variants and the `SHRI` opcode.
- Packed unions of bitfields occupy only the bytes their bits need
  (`{ unsigned a:3; unsigned int b:17; }` is 3 bytes, not 4), matching GCC.
- `table.new` rejects negative sizes and sizes above `INT_MAX` with a clear
  error instead of silently ignoring or wrapping them.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```

New and expanded tests:

- `tests/jit/126_const64.lua`
- `tests/jit/127_exact_cmp.lua`
- `tests/jit/128_bitwise_loops.lua`
- `tests/ffi/100_packed_union_bits.lua`
- `tests/compat/extensions.lua`
