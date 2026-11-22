# Luar 0.118 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant-float field writes through table upvalues.

## Highlights

- `t.x = 2.5` where `t` is a closed-over table compiles as `SETTABUP`.
- New `TSETUPKF` helper resolves the current closure at runtime and applies
  `luaH_set` directly, avoiding a scratch stack slot.
- `tests/jit/109_trace_settabup_field.lua` checks writing and reading back
  `2.5`, forced GC and `alloc=0`.

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

- Only constant-float upvalue field writes; int/string/table values fall
  back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
