# Luar 0.230 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds raw 64-bit cdata fills.

## Highlights

- `dst[i] = v` fills a bounded `int64_t`/`uint64_t` array from an
  int64/uint64 cdata scalar with a raw 8-byte store.
- Register and closure-upvalue values and destinations are supported,
  including strided and descending loops.
- Out-of-bounds runs fall back to the interpreter and keep raising.

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

- `tests/ffi/192_j2_i64_fill.lua`

## Known limitations

- The source must be a scalar cdata int64/uint64 value; Lua integers and
  mixed element widths use the existing numeric/fallback paths.
- Fills require bounded 8-byte integer arrays; pointers fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
