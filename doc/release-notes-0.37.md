# Luar 0.37 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
handles computed pointer indexes in J4.

## Highlights

- **Computed pointer indexes**: `o.ptr[i + 1]` recognizes the
  `GETFIELD`/`ADDI`/`MMBINI`/`GETTABLE` sequence, emits the key addition in
  IR and folds the pointer-field load.
- `tests/ffi/22_trace_cdata.lua` covers `s.p[i + 1]`.
- Pointer arithmetic still means index addition; arbitrary cdata pointer
  arithmetic remains interpreter-side.

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

- Arbitrary pointer arithmetic and multi-step offsets fall back.
- `B == 0` calls and continuation error/yield paths remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
