# Luar 0.8 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
hardens J4 exit-state handling and closes the macOS shared-install gap.

## Highlights

- **64-slot spill/restore stress**: the IR exit snapshot selftest now fills
  all 64 supported slots with mixed integer/float values, rejects over-cap
  snapshots, zeroes the frame and verifies exact restoration.
- **TSET fallback regression**: growing table writes exercised under `ir=1`
  must fall back without corruption (`tests/jit/62_exit_spill.lua`).
- **macOS shared ABI/install**: CI now links and runs the embed example
  against `libluar.dylib`, then performs a staged install and links a second
  embed against the installed library through `pkg-config`.
- J4 suite now has 61 files.

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

GitHub Actions additionally runs the macOS optional job with the new
shared/install checks.

## Known limitations

- Side chains still exclude `TSET`; growing writes use interpreter fallback.
- Blocked-PC tracking is bitmap-based and conservative.
- Native exit-stub spilling remains C-level snapshot/restore.
- LuaR/Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
