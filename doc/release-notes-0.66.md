# Luar 0.66 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant-index table access to J4 traces.

## Highlights

- **`GETI`**: `t[1]` reads the array part directly, with a `luaH_getint`
  fallback for hash cells; no allocation.
- **`SETI`**: `t[1] = i` reuses the relocation-safe insert helper; the
  emitter reloads the relocated base after growth.
- `tests/jit/77_trace_geti_seti.lua` covers compile/execute, forced GC and
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

- `GETI`/`SETI` cover integer values; float/other constant-key cells remain
  interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
