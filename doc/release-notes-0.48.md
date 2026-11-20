# Luar 0.48 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds runtime two-term pointer-offset reads.

## Highlights

- **Runtime offsets**: `(p + i + j)[0]` recognizes two consecutive pointer
  additions, emits an integer `ADD` and folds the pointer-field read.
- `tests/ffi/22_trace_cdata.lua` covers the runtime two-term case.
- Fully arbitrary expression trees still fall back.

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

- More than two runtime pointer terms fall back.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
