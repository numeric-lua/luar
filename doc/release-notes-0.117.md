# Luar 0.117 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds field reads through table upvalues.

## Highlights

- `t.x` where `t` is a closed-over table compiles as Lua 5.5’s `GETTABUP`.
  The recorder materializes the upvalue into the destination slot and then
  emits the existing `TGETK*` field op.
- `tests/jit/108_trace_gettabup_field.lua` covers a float field, forced GC
  and `alloc=0`.
- Runtime field/tag mismatches exit through the existing helper validation.

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

- Upvalue field writes remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
