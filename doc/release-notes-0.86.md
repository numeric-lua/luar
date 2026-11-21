# Luar 0.86 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
generalizes trace concatenation to 2–8 operands.

## Highlights

- `a .. b .. c .. d` lowers to `CONCATN` with the operand count.
- Strings and numbers are formatted with the interpreter's
  `luaO_tostringbuff`; the result is allocated once with `luaS_newlstr`.
- `tests/jit/90_trace_concatn.lua` covers four strings and mixed
  numbers/strings plus forced GC.

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

- Up to eight operands; longer chains fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
