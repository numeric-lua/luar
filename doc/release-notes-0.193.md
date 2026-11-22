# Luar 0.193 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes four J2 fast-path miscompiles and two FFI layout edge cases against
the C compiler.

## Highlights

- J2 now rejects loops whose registers change type between iterations.
  A branch that turned a float local into an integer (or the reverse) could
  make the next native iteration reinterpret the payload; such loops fall
  back to the interpreter.  Type-stable branch loops still compile.
- The fast `while`/`repeat` paths no longer cache a limit that the body also
  uses as the accumulator or induction variable.  These shapes use the
  generic emitter, which reloads the slot every iteration.
- Every fast path now writes its intermediate result back to the bytecode
  register: `i*i`, `i*const`, a raw table element and an inlined `__index`
  result stay readable from a local after the loop.  The accumulated sums
  were already correct; the temporaries were stale.
- A zero-width bitfield advances to its base type's natural boundary even
  in `__attribute__((packed))` and `#pragma pack` structs, so following
  fields and the struct size match GCC.
- `#pragma pack(n)` packs consecutive bitfields with the same tight rules
  as `pack(1)`; ordinary members and aggregate alignment remain capped by
  `n`.  `pack(2)`/`pack(8)` no longer reserve whole base-type units.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make shared test-shared
tools/upstream-check.sh
make dist && make test-dist
```

New tests:

- `tests/jit/129_type_stable.lua`
- `tests/jit/130_fast_alias.lua`
- `tests/jit/131_fast_temp.lua`
- `tests/ffi/101_zero_width_bitfields.lua`
- `tests/ffi/102_pragma_pack_bits.lua`
