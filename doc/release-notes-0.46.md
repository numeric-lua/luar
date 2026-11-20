# Luar 0.46 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
generalizes constant pointer-offset chains for reads.

## Highlights

- **Arbitrary constant chains**: `(p + i + 1 + 1 + 1 + 1)[0]` scans all
  consecutive `ADDI`/`MMBINI` steps, sums their offsets and emits one pointer
  field read.
- `tests/ffi/22_trace_cdata.lua` covers a four-step chain.
- Writes still use the two-step special case.

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

- Pointer-arithmetic writes are limited to two constant steps.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
