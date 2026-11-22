# Luar 0.130 release notes

This release refreshes the performance gate at 0.129.

## Highlights

- `doc/performance.md` records current `./bench/compare.sh` numbers.
- Luar remains fastest on integer/table-array loops, ties `product`, and
  loses recursive `fib` and hash insertion.
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
