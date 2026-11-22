# Luar 0.120 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant string writes through table upvalues.

## Highlights

- `t.x = "hello"` where `t` is a closed-over table traces.
- `tests/jit/111_trace_settabup_string_field.lua` checks write/read, forced
  GC and `alloc=0`.
- Proto constants keep the string rooted.

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

- Upvalue field writes accept constant int/float/string only.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
