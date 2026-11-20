# Luar 0.31 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds the 8-byte integer pointer-element path.

## Highlights

- **8-byte pointer elements**: `long *`/`int64_t *` struct fields use
  `CPGETI64`/`CPSETI64` rather than the 4-byte path.
- `tests/ffi/22_trace_cdata.lua` checks a `long *` field write/read loop.
- The FFI trace matrix now covers 1/2/4/8-byte integers and 4/8-byte floats
  through cdata arrays, direct fields, nested fields and pointer fields.

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

- Pointer arithmetic beyond indexing still falls back.
- `B == 0` calls and continuation error/yield paths remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
