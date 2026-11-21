# Luar 0.75 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant-integer field writes to traced table constructors.

## Highlights

- `{ x = 1 }` lowers `SETFIELD` with the `k` flag to `TSETKK`, writing the
  interned string key and immediate integer value.
- The write copies the table value, uses `luaH_set`, and returns the
  relocated frame base after a possible resize.
- `tests/jit/81_trace_constant_field.lua` covers tracing, forced GC and
  `alloc=0` fallback.

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

- Constant writes are integer-only; string/float/table values fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
