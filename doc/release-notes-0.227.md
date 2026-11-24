# Luar 0.227 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
widens affine/index table fills.

## Highlights

- Affine/index fills now accept any nonzero integer step, including
  strided (`for i = 1, n, 3`) and descending (`for i = n, 1, -2`) loops.
- Scalars can be register locals or closure upvalues, as well as small
  Proto constants.
- A float scalar promotes results to floats, matching the interpreter:
  `t[i] = i + 2.5`, `t[i] = i - 2.5`, `t[i] = i * 2.5`.
- The same forms work with a closure-upvalue destination.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make test-shared
make test-dist
tools/upstream-check.sh
```

Extended test:

- `tests/jit/221_table_affine_fill.lua`

## Known limitations

- Counts are capped at `1 << 22` iterations; a zero step is rejected.
- Float promotion uses double arithmetic; integer results stay in the
  wrap-defined Lua integer path.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
