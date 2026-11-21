# Luar 0.87 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds string length to J4 traces.

## Highlights

- `#s` lowers to `STRLEN` when the live operand is a string; the helper
  returns `tsslen` as an integer with no allocation.
- `tests/jit/91_trace_strlen.lua` covers local strings, forced GC and the
  `alloc=0` path.
- String upvalues with generic `OTHER` guards still fall back.

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

- String upvalues that require generic tag guards remain interpreter-side.
- Table length (`#t`) remains interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
