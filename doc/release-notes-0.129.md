# Luar 0.129 release notes

This release updates the J4 native-call documentation.

## Highlights

- `doc/ffi.md` now lists `CCALL0D` and all mixed two-argument forms:
  `CCALL2ID`, `CCALL2FI`, `CCALL2IF`, `CCALL2FD`, `CCALL2DF`.
- The outdated float32 cdata-array limitation is removed; the 64-bit cdata
  field boundary is documented.
- No runtime behavior change.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```
