# Luar 0.124 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds nil deletion through table upvalues.

## Highlights

- `t.x = nil` where `t` is a closed-over table traces.
- `TSETUPKN` uses `luaH_set` with a nil value; `rawget` returns nil.
- `tests/jit/115_trace_settabup_nil_field.lua` checks deletion, forced GC
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

- Metatable `__newindex` paths remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
