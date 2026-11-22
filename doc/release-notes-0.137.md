# Luar 0.137 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds J4 entry diagnostics.

## Highlights

- `jit.stats()` now reports `ir_entry_checks`,
  `ir_entry_pc_mismatch` and `ir_entry_base_mismatch`.
- These distinguish a trace that never sees its call site from one that
  enters and exits.
- `tests/jit/123_ir_entry_stats.lua` covers a working trace.

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
