# Luar 0.232 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds raw 64-bit cdata multiply reductions.

## Highlights

- `acc = acc * arr[i]` over bounded int64/uint64 arrays uses wrap-defined
  raw 64-bit multiplication with a cdata accumulator.
- int64/uint64 accumulators, strided and descending loops, and upvalue
  arrays are supported.
- Results are compared against the interpreter for overflow/wrap cases.

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

- `tests/ffi/193_j2_i64_sum.lua`

## Known limitations

- Divide, modulo and bitwise 64-bit reductions remain interpreter-side.
- Bounded 8-byte arrays only; pointer arrays fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
