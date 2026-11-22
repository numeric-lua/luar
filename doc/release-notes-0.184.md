# Luar 0.184 release notes

This release hardens the raw int64 cdata copy path added in 0.183.

## Highlights

- Pointer cdata sources/destinations use the pointee address, not the cdata
  header.
- Copies use `memmove`, so overlapping and self copies are safe.
- Only the canonical `dst[k] = src[k]` shape fuses; arithmetic results,
  discarded loads, constants and intervening instructions abort the trace.
- A failed helper resumes at the original load PC, so interpreter errors and
  untouched destination data are preserved.
- `tests/ffi/86_int64_copy_guards.lua` and
  `tests/ffi/87_int64_copy_pointer.lua` cover the guard matrix.

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
