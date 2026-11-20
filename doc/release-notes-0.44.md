# Luar 0.44 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes constant-offset pointer arithmetic for both integer and double
elements.

## Highlights

- **Constant-offset double writes**: `(o.q + i + 1)[0] = 1.5` lowers to
  `CPSETF` through the full pointer-arithmetic sequence.
- The floatify path now permits integer key-control ops while still
  converting float arithmetic.
- `tests/ffi/22_trace_cdata.lua` covers integer and double constant-offset
  pointer reads/writes.

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

- Arbitrary multi-step pointer arithmetic falls back.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
