# Luar 0.157 release notes

This release corrects the documented status of `CCALL2FF`.

## Highlights

- `float(float,float)->float` calls were listed as native, but the
  `CCALL2FF` probe rejects valid function cdata, so no trace installs.
- `doc/ffi.md` now records this as a known J4 boundary instead of claiming
  support.
- The recorder reaches the candidate branch (argument proof succeeds), but
  the probe returns 0 before the ctype checks; this is tracked for a
  follow-up fix.
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
