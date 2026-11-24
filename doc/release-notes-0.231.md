# Luar 0.231 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds raw 64-bit cdata reductions.

## Highlights

- `acc = acc +/- arr[i]` over bounded `int64_t`/`uint64_t` arrays uses raw
  8-byte add/subtract with a cdata accumulator.
- Register and closure-upvalue arrays, strided and descending loops are
  covered.
- The accumulator keeps its cdata type; the element temporary is a cdata
  object of the array element type.
- Out-of-bounds runs fall back and raise from the interpreter.

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

- `tests/ffi/193_j2_i64_sum.lua`

## Known limitations

- Only add/subtract are supported; multiply/divide/bitwise reductions stay
  interpreter-side.
- Fills/copies require bounded 8-byte arrays; pointer arrays fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
