# Luar 0.131 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes integer dynamic-key float writes.

## Highlights

- `t[k] = 2.5` previously fell into the integer `TSET` path and stored the
  loop count or bits; it now uses `TSETDYN_KF`.
- `t[k] = v` with a float register uses the generalized `TSETDYN` helper.
- `tests/jit/117_trace_dynamic_int_key_float.lua` covers both, forced GC and
  `alloc=0`.

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

- Constant string/int/bool/table values on dynamic SETTABLE still fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
