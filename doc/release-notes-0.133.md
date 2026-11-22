# Luar 0.133 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant 64-bit integer values for dynamic-key writes.

## Highlights

- `t[k] = 1099511627777` uses `TSETDYN_KI`; the value bits travel in
  `nval`, so no double truncation occurs.
- `tests/jit/119_trace_dynamic_key_int.lua` checks write/read, forced GC
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

- Constant bool/table values on dynamic SETTABLE still fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
