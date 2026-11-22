# Luar 0.144 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends dynamic-key writes to integer register values on non-empty tables.

## Highlights

- `t[k] = 7` on a table with existing hash/array content now traces through
  `TSETDYN`.
- Empty tables keep the existing side-trace-safe insertion path.
- `tests/jit/125_trace_dynamic_int_value_nonempty.lua` checks write/read,
  forced GC and `alloc=0`.

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
