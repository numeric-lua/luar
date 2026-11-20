# Luar 0.36 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release fixes
J4 installation statistics.

## Highlights

- **Truthful compile counters**: rejected recordings no longer emit a
  throw-away code buffer or increment `ir_compiled`/`ir_code_bytes`.
- New `ir_install_rejects` accounts for recorded traces that could not be
  installed.
- `tests/jit/38_ir_recorder.lua` now asserts the new contract.

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

- `B == 0` calls and continuation error/yield paths remain unsupported.
- Pointer arithmetic beyond indexing falls back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
