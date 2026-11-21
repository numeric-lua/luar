# Luar 0.103 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
raises the trace IR budget for large constructors.

## Highlights

- The IR budget grows from 256 to **1024 ops**.
- A 100-element list constructor plus loop control records and executes
  natively.
- `tests/jit/102_trace_large_list.lua` covers 100 elements, forced GC and
  `alloc=0`.

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

- 200+ element constructors still exceed the slot/trace limits.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
