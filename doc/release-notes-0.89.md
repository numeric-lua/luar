# Luar 0.89 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
raises trace concatenation to 32 operands.

## Highlights

- `CONCATN` accepts 2–32 operands; the helper formats strings/numbers and
  allocates the result once.
- `tests/jit/93_trace_concat32.lua` covers a ten-part chain, forced GC and
  the `alloc=0` path.

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

## Known limitations

- More than 32 operands fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
