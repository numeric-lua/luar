# Luar 0.234 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds raw 64-bit divide/modulo cdata reductions.

## Highlights

- `acc = acc // arr[i]` and `acc = acc % arr[i]` over bounded int64/uint64
  arrays use raw cdata arithmetic with a cdata accumulator.
- Signed division truncates toward zero; `INT64_MIN / -1` wraps.
- A zero divisor is pre-scanned before touching the accumulator, so the
  whole run falls back to the interpreter's cdata behavior.
- Strided/descending runs are covered.

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

New test:

- `tests/ffi/195_j2_i64_divmod_sum.lua`

## Known limitations

- Bounded 8-byte arrays only; pointer arrays fall back.
- Zero divisors/`INT64_MIN` edge cases use the interpreter path.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
