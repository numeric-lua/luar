# Luar 0.41 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes the first pointer-arithmetic read/write pair in J4.

## Highlights

- **Pointer-arithmetic writes**: `(o.ptr + i)[0] = i` and
  `(o.q + i)[0] = 2.5` recognize the
  `GETFIELD`/`ADD`/`MMBIN`/`SETI` sequence and emit `CPSETI`/`CPSETF`.
- Null pointers and unsupported element types fall back.
- `tests/ffi/22_trace_cdata.lua` covers pointer arithmetic reads and writes.

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

- Multi-step pointer arithmetic and non-zero dereference offsets fall back.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
