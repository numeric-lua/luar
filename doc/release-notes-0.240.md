# Luar 0.240 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant in-place updates through upvalue-array destinations.

## Highlights

- `U[i] = U[i] OP k` over a closure-upvalue int64/uint64 array supports
  add/subtract/multiply/bitwise/IDIVK/MODK and masked shifts.
- Constants are int32 immediate/K values.
- The upvalue array is reloaded by the first interpreted iteration and
  the helper keeps the cdata element temporary.

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

Extended test:

- `tests/ffi/196_j2_i64_update.lua`

## Known limitations

- Upvalue-array destinations with cdata/register scalars still fall back.
- Constants outside int32 remain interpreter-side.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
