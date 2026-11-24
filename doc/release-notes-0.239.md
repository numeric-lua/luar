# Luar 0.239 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds upvalue-cdata-scalar in-place 64-bit updates.

## Highlights

- `a[i] = a[i] +|-|*|&|||~|<<|>> V` over bounded int64/uint64 arrays
  accepts a closure-upvalue cdata scalar.
- The upvalue is re-resolved on each entry, so rebinding it is observed.
- int64/uint64 destinations and masked shift counts are covered.

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

- Register cdata scalars, constants and upvalue scalars are covered;
  upvalue-array destinations still fall back.
- Bounded 8-byte arrays only; pointer arrays fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
