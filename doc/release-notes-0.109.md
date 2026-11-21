# Luar 0.109 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a benchmark for the new FADDI trace path.

## Highlights

- `j4/float_addi` exercises `s = s + (v + 1)` and measured **1.83x**
  JIT-on versus JIT-off in `make bench-jit`.
- `doc/performance.md` records the result and its microbenchmark caveat.
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
