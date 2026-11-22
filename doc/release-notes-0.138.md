# Luar 0.138 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends J4 entry diagnostics with the run result.

## Highlights

- `jit.stats()` now reports `ir_entry_failures` and
  `ir_last_entry_result` (the last `luari_run_j4` return code).
- The FADDI call trace was confirmed to enter and return `-11`
  (an exit at PC 9) instead of continuing.
- `tests/jit/124_ir_entry_result.lua` checks a working trace reports no
  failures and a positive result.

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
