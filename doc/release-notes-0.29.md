# Luar 0.29 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds pointer-field element reads to the FFI trace subset.

## Highlights

- **Pointer-field reads**: `o.ptr[i]` and `o.dptr[i]` fold the field load and
  element read into `CPGETI`/`CPGETF`.
- Null pointers and unsupported element types fall back to the interpreter.
- `tests/ffi/22_trace_cdata.lua` covers int and double pointer fields.
- The FFI trace matrix now spans cdata arrays, direct/nested struct fields
  and pointer-field elements.

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

- Pointer-field writes and pointer arithmetic still fall back.
- 8-byte integer pointer elements are not yet handled by the narrow helper.
- `B == 0` calls and continuation error/yield paths remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
