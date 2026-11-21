# Luar 0.71 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
the first JIT-on/off performance report.

## Highlights

- `make bench-jit` runs the `j4` benchmark group twice (JIT on, then
  `LUAR_BENCH_NOJIT=1`) and prints median CPU time plus speedup per benchmark.
- This is a report, not a hard gate; regressions such as `table_hash_side`
  are visible rather than hidden.
- `doc/testing.md` documents the target.

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

## Known limitations

- Benchmark numbers are machine-dependent and not asserted.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
