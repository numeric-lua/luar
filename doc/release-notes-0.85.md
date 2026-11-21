# Luar 0.85 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends trace concatenation to three operands.

## Highlights

- `a .. b .. c` lowers to `CONCAT3` and formats strings/numbers with the
  same `luaO_tostringbuff` path as the interpreter.
- `tests/jit/89_trace_concat3.lua` covers all-string and mixed
  number/string/float cases plus forced GC.
- Chains longer than three operands still fall back.

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

- Maximum three concat operands in one trace op; longer chains fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
