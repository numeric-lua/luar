# Luar 0.229 release notes

This is a correctness patch for the raw 64-bit cdata copy helper.

## Fixes

- The visible element temporary was created as an owned-payload cdata,
  but int64/uint64 cdata values live inline in `cd->v`.  The temporary
  therefore contained pointer bits instead of the last copied element.
  It is now a scalar cdata with the value stored inline.
- After allocating the temporary, the helper reloads the frame base
  before writing the slot, avoiding a stale stack pointer if the
  allocation moved the stack.
- Added edge coverage for int64→uint64 destinations, self-copy and
  out-of-bounds fallback.

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

- `tests/ffi/191_j2_i64_copy.lua`

## Known limitations

- The helper still allocates one cdata temporary per entry.
- Raw copies require bounded 8-byte integer arrays; pointers and mixed
  widths fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
