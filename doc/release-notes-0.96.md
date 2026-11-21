# Luar 0.96 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends traced list constructors to float and string elements.

## Highlights

- `{ 1, 2.5, "x" }` lowers to `TSETI`/`TSETIF`/`TSETIS` with `KSTR` for the
  string constant.
- The helpers use `luaH_setint`, reload the relocated base, and validate
  element tags at runtime.
- `tests/jit/98_trace_list_elements.lua` covers the mixed list, forced GC
  and `alloc=0`.

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

- Struct/pointer cdata field chains remain interpreter-side (cdata
  arithmetic is not modelled).
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
