# Luar 0.236 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds 64-bit divide/modulo in-place cdata updates.

## Highlights

- `a[i] = a[i] // k` and `a[i] = a[i] % k` over bounded int64/uint64
  arrays use raw cdata arithmetic with a cdata scalar.
- Signed division truncates toward zero; `INT64_MIN / -1` wraps.
- A zero divisor uses the interpreter's cdata result (`INT64_MIN` bit
  pattern), matching the existing cdata semantics.

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

- Bounded 8-byte arrays only; pointer arrays fall back.
- The helper still creates one cdata element temporary per entry.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
