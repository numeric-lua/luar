# Luar 0.228 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds raw 64-bit cdata element copies.

## Highlights

- `dst[i] = src[i]` for bounded `int64_t`/`uint64_t` arrays now copies
  eight-byte elements directly instead of falling back to the Lua-number
  store path.
- The visible element temporary is a cdata object, matching the
  interpreter's cdata result.
- Register and closure-upvalue sources/destinations, mixed signedness and
  strided/descending loops are covered.

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

- `tests/ffi/191_j2_i64_copy.lua`

## Known limitations

- The raw copy helper creates one cdata element temporary per entry; other
  64-bit reads still stay interpreter-side.
- Source and destination must be bounded 8-byte integer arrays; pointers
  and mixed element widths fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
