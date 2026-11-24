# Luar 0.233 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds raw 64-bit bitwise/shift cdata reductions.

## Highlights

- `acc = acc &|~|<<|>> arr[i]` over bounded int64/uint64 arrays uses raw
  cdata arithmetic with a cdata accumulator.
- Shift counts follow the cdata mask-to-six-bits rule, including negative
  and 64-plus counts.
- int64/uint64 accumulators, strided/descending loops and upvalue arrays
  are supported.

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

- `tests/ffi/194_j2_i64_bitwise_sum.lua`

## Known limitations

- Divide and modulo 64-bit reductions remain interpreter-side.
- Bounded 8-byte arrays only; pointer arrays fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
