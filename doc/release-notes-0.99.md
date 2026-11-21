# Luar 0.99 release notes

This release records the 0.98 performance gate.

## Highlights

- `doc/performance.md` now carries a current `./bench/compare.sh` snapshot
  against stock Lua 5.5.1 and LuaJIT 2.1 on the Ryzen 3 7320U host.
- Luar wins integer sum, float sum and table array loops; ties `product`;
  loses recursive `fib`, `mandel`, float loops and hash cases.
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
