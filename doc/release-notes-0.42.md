# Luar 0.42 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the pointer-arithmetic fold by one constant offset.

## Highlights

- **Constant-offset pointer reads**: `(o.ptr + i + 1)[0]` recognizes the
  `GETFIELD`/`ADD`/`MMBIN`/`ADDI`/`MMBINI`/`GETI` sequence and folds it into
  a pointer-field read over `i + 1`.
- `tests/ffi/22_trace_cdata.lua` covers this read.
- Multiple constants or arbitrary offset expressions still fall back.

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

- Pointer-arithmetic writes with a constant offset fall back.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
