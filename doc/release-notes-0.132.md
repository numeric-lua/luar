# Luar 0.132 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant string values for dynamic-key writes.

## Highlights

- `t[k] = "hi"` now uses `TSETDYN_KS` instead of the integer TSET path.
- `tests/jit/118_trace_dynamic_key_string.lua` checks write/read, forced GC
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

- Constant int/bool/table values on dynamic SETTABLE still fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
