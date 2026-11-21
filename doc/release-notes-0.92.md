# Luar 0.92 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
connects cdata-array upvalues to native C calls and cdata reads.

## Highlights

- `sqrtf(arr[i])` and `fabs(arr[i])` now trace when `arr` is a closed-over
  cdata array.  The recorder tracks the upvalue's array identity and emits
  a CGUARD plus a runtime integer-key guard.
- `TAG_USERDATA` is deliberately restricted to cdata **arrays**; pointer and
  struct cdata keep the previous `TAG_OTHER` interpreter fallback, avoiding
  the CPGETI64 path regression.
- `tests/ffi/38_trace_cdata_array_upvalue.lua` covers float32/double calls,
  forced GC, and replacing the array upvalue with a Lua table.

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

- Struct/pointer cdata upvalues remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
