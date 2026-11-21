# Luar 0.95 release notes

This release documents the cdata-arithmetic boundary in J4.

## Highlights

- `doc/ffi.md` explains that `long`/`int64_t` reads yield cdata int64, so
  `x = x + s.lp[i]` accumulates cdata.
- J4 has no cdata arithmetic IR; struct/pointer field chains remain
  interpreter-side.
- `TAG_USERDATA` stays restricted to cdata arrays, which are safe for
  native float/int reads and C-call arguments.
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
