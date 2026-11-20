# Luar 0.26 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
removes the last runtime use of the C-side rollback snapshot in J4 side
chains.

## Highlights

- **Memory-form exit state**: J4 writes every result directly to the Lua
  stack; side-chain fallback now resumes from the deepest exact node start
  without restoring a C copy.
- **No native spill stub**: a generated register-spill buffer would duplicate
  state already present in the Lua stack.  The design and rationale are now
  documented in `doc/j4-tracing-ir.md`.
- `LuaR_IRExitState` remains trace metadata and a selftest vehicle, not a
  runtime spill buffer.
- Existing 64-slot snapshot stress, exact-PC, mixed-cell and side-chain tests
  remain green.

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

GitHub Actions also runs the macOS shared/install/module job.

## Known limitations

- Continuations that consume the call result and `B == 0` calls still fall
  back.
- Deeper than two-level field paths and pointer fields fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
