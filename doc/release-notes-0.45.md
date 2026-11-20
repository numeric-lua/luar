# Luar 0.45 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a two-constant pointer-offset read.

## Highlights

- **Two-step pointer offsets**: `(p + i + 1 + 1)[0]` recognizes both
  `ADDI`/`MMBINI` steps, folds their sum into the element index and emits a
  pointer-field read.
- `tests/ffi/22_trace_cdata.lua` covers the two-step read.
- Longer chains still fall back.

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

- More than two constant pointer steps fall back.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
