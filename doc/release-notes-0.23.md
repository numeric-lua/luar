# Luar 0.23 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
folds two-level scalar struct-field reads into a single FFI trace op.

## Highlights

- **Nested scalar field reads**: `o.inner.x` is resolved at record time and
  executed as one `CGETFIELD` with the combined offset.
- Bitfields, deeper nesting, pointer fields and aggregate writes still fall
  back.
- `tests/ffi/22_trace_cdata.lua` covers the nested read alongside arrays,
  copies, direct fields and constant writes.

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

- Deeper than two levels and nested writes fall back.
- C calls remain CALL exits, not native continuations.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
