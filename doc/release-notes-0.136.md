# Luar 0.136 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends dynamic-key register values to strings and booleans.

## Highlights

- `t[k] = s` and `t[k] = b` now use `TSETDYN`.
- Boolean upvalue ULOADs have no boolean IR guard tag, so the recorder skips
  that guard and relies on runtime value validation.
- `tests/jit/122_trace_dynamic_key_string_bool.lua` covers both values,
  forced GC and `alloc=0`.

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

- Integer register values remain on the side-trace-safe fallback.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
