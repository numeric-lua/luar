# Luar 0.226 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends table fill coverage with direct affine/index matchers.

## Highlights

- `t[i] = i`, `t[i] = i + k`, `t[i] = i - k` and `t[i] = i * k` are
  recognised directly and run through the affine/index helper.
- The same forms work with a closure-upvalue destination:
  `U[i] = i`, `U[i] = i + k`, `U[i] = i - k`, `U[i] = i * k`.
- The direct matchers cover shapes the general decoder did not select,
  including upvalue destinations; constants are small integer values.

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

New tests:

- `tests/jit/221_table_affine_fill.lua`
- `tests/jit/220_table_generic_keys.lua` (upvalue identity fill)

## Known limitations

- Affine fills require step 1, a positive loop range and a small integer
  constant; other forms stay interpreter-side.
- Upvalue destinations are refreshed by the first interpreted iteration
  and re-checked by the helper.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
