# Luar 0.102 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
supports large list constructors in traces.

## Highlights

- `SETLIST` with the `k` flag and a following `EXTRAARG` now records the
  real starting index, so constructors with more than 50 elements trace.
- The IR budget grows from 64 to 256 ops, enough for a 60-element
  constructor plus loop control.
- `tests/jit/102_trace_large_list.lua` covers a 60-element list, forced GC
  and `alloc=0`.

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

- Larger constructors still depend on the 256-op budget.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
