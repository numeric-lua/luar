# Luar 0.123 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
generalizes register writes through table upvalues.

## Highlights

- `t.x = v` where `t` is a closed-over table and `v` is any non-nil
  register value traces through `TSETUPKV`.
- `tests/jit/114_trace_settabup_dynamic_field.lua` covers float and string
  values, forced GC and `alloc=0`.

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

- Nil deletion and metatable `__newindex` remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
