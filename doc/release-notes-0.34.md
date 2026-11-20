# Luar 0.34 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
generalizes nested scalar field reads.

## Highlights

- **Generic field-path reads**: the recorder collects a chain of up to eight
  `GETFIELD` opcodes, resolves the combined offset with one helper, and emits
  a single `CGETFIELD`.
- This subsumes the previous two-, three- and four-level special cases.
- `tests/ffi/22_trace_cdata.lua` adds a five-level read.
- Writes remain limited to four levels.

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

- Reads deeper than eight levels fall back.
- Four-level writes remain the write limit.
- Pointer arithmetic beyond indexing falls back.
- `B == 0` calls and continuation error/yield paths remain unsupported.
