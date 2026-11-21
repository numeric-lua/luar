# Luar 0.84 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends trace concatenation to number operands.

## Highlights

- `1 .. "a"` and `1.5 .. 2` trace through `CONCAT2`.
- Numbers are formatted with Lua's own `luaO_tostringbuff` before the
  result string is allocated, so formatting matches the interpreter.
- `tests/jit/88_trace_concat_numbers.lua` covers integer/string and
  float/integer cases, plus forced GC.

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

- Exactly two operands; longer concat chains fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
