# Luar 0.238 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant divide/modulo in-place 64-bit cdata updates.

## Highlights

- `a[i] = a[i] // k` and `a[i] = a[i] % k` over bounded int64/uint64
  arrays accept int32 immediate/K constants.
- Signed division truncates toward zero; zero constants use the
  interpreter cdata result (`INT64_MIN` bit pattern).
- int64/uint64 destinations are covered.

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

- Constants outside int32 remain interpreter-side.
- Bounded 8-byte arrays only; pointer arrays fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
