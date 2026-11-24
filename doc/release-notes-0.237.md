# Luar 0.237 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds small-integer-constant in-place 64-bit cdata updates.

## Highlights

- `a[i] = a[i] +|-|*|&|||~|<<|>> k` over bounded int64/uint64 arrays
  accepts immediate/K integer scalars.
- Shift counts use the cdata mask-to-six-bits rule.
- K scalars are restricted to the int32 range because they are stored in
  the tint field.

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

Extended tests:

- `tests/ffi/196_j2_i64_update.lua`
- `tests/ffi/98_int64_shift.lua` (tint-disabled J4 baseline)
- `tests/ffi/145_j2_cdata_i64_store.lua` (expects the i64 helper)

## Known limitations

- K scalars outside int32 remain interpreter-side.
- Divide/modulo constant updates are not included.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
