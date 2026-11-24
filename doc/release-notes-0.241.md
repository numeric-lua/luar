# Luar 0.241 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds cdata-scalar updates through upvalue-array destinations.

## Highlights

- `U[i] = U[i] +|-|*|&|||~|<<|>> V` over a closure-upvalue int64/uint64
  array accepts a cdata scalar register or upvalue.
- Scalar and array validation is deferred to the runtime helper, avoiding
  record-time dereferences of stale stack values.
- Constants and cdata scalars share the same 64-bit update helper.

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

- Upvalue-array destinations with complex expression scalars still fall
  back.
- Bounded 8-byte arrays only; pointer arrays fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
