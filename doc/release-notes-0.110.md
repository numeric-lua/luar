# Luar 0.110 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a long-loop benchmark to the J4 gate.

## Highlights

- `j4/long_loop` runs a 220-statement body, exercising the FORLOOP `Bx`
  recording fix.
- `make bench-jit` measured **4.13x** JIT-on versus JIT-off; `j4/float_addi`
  measured 1.50x in the same run.
- `doc/performance.md` records both microbenchmark results.
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
