# Luar 0.20 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
closes the constant-integer case for scalar struct-field writes.

## Highlights

- **Constant field writes**: `CSETFIELDK` stores an integer constant into a
  scalar signed integer struct/union field (`p.x = 7`).
- Field offsets and sizes are resolved at record time; the helper validates
  cdata identity and bounds.
- `tests/ffi/22_trace_cdata.lua` now covers array read/write, struct field
  read/write and constant field writes.
- Float constant writes and constant-RK pointer/nested fields still fall
  back.

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

- Float constant field writes and nested aggregates fall back.
- C calls and complex cdata values remain interpreter-side.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
