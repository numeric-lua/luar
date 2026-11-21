# Luar 0.78 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant-float table field writes.

## Highlights

- `{ x = 2.5 }` lowers `SETFIELD` with the `k` flag to `TSETKKF`; the
  interned key and double immediate are stored through `luaH_set`.
- The write returns the relocated base after growth; GC stress and
  `alloc=0` fallback are tested.
- `tests/jit/83_trace_constant_float_field.lua`.

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

- Constant field values are int/float only; strings/tables fall back.
- Float field reads (`TGETKF`) remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
