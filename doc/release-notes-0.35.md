# Luar 0.35 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
generalizes nested scalar field writes to the same path model as reads.

## Highlights

- **Generic field-path writes**: the recorder collects up to eight chained
  `GETFIELD` opcodes followed by `SETFIELD`, resolves the combined offset and
  emits one `CSETFIELD`/`CSETFIELDK`.
- This subsumes the earlier two-, three- and four-level write special cases.
- `tests/ffi/22_trace_cdata.lua` adds five-level writes.
- Reads and writes now share the same eight-level field-path limit.

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

- Paths deeper than eight levels fall back.
- Pointer arithmetic beyond indexing falls back.
- `B == 0` calls and continuation error/yield paths remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
