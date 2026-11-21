# Luar 0.72 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
allocation observability.

## Highlights

- `jit.stats().ir_alloc_tables` counts trace-side `TNEW` allocations,
  including sized table constructors.
- `tests/jit/74_trace_tnew.lua` asserts the counter advances.
- No behavior change beyond the counter.

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

- The counter is reset by `jit.flush()` and does not count interpreter-side
  allocations.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
