# Luar 0.40 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
handles a first form of cdata pointer arithmetic in J4.

## Highlights

- **Pointer arithmetic reads**: `(o.ptr + i)[0]` recognizes the
  `GETFIELD`/`ADD`/`MMBIN`/`GETI` sequence and folds it into the pointer-field
  read with `i` as the element index.
- `tests/ffi/22_trace_cdata.lua` covers the pointer-arithmetic read.
- General multi-step pointer arithmetic still falls back.

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

- Pointer arithmetic writes and multi-step offsets fall back.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
