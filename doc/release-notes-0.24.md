# Luar 0.24 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
folds two-level scalar struct-field writes into a single FFI trace op.

## Highlights

- **Nested scalar field writes**: `o.inner.x = i` and `o.inner.y = 2.5` are
  resolved at record time and executed as one `CSETFIELD`/`CSETFIELDK` with
  the combined offset.
- The recorder skips the temporary `GETFIELD`/`SETFIELD` pair after folding.
- Bitfields, pointer fields, deeper nesting and aggregate values still fall
  back.
- `tests/ffi/22_trace_cdata.lua` covers nested reads and writes.

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

- Deeper than two levels falls back.
- C calls remain CALL exits, not native continuations.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
