# Luar 0.77 release notes

This release records the J4 native C-call matrix.

## Highlights

- `doc/ffi.md` lists every native call signature J4 executes, its IR op and
  an example.
- The documented boundaries include the float32 cdata-read install gap,
  `B == 0`, varargs/aggregates, and `ffi.new`.
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
