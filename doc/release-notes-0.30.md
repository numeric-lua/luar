# Luar 0.30 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes pointer-field element access for the FFI trace subset.

## Highlights

- **Pointer-field writes**: `o.ptr[i] = v` and `o.dptr[i] = x` fold the field
  load and element store into `CPSETI`/`CPSETF`.
- Null pointers and unsupported element types fall back.
- `tests/ffi/22_trace_cdata.lua` covers pointer-field reads and writes.
- The FFI trace matrix now covers cdata arrays, direct/nested/three-level
  fields, pointer-field elements and constant writes.

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

- Pointer arithmetic beyond indexing and 8-byte integer pointer elements
  fall back.
- `B == 0` calls and continuation error/yield paths remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
