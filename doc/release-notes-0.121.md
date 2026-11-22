# Luar 0.121 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant boolean writes through table upvalues.

## Highlights

- `t.x = true` where `t` is a closed-over table traces.
- `tests/jit/112_trace_settabup_bool_field.lua` checks write/read, forced GC
  and `alloc=0`.
- Scalar upvalue field writes now cover int, float, string and boolean.

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

- Table-valued upvalue field writes remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
