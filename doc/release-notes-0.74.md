# Luar 0.74 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends native calls to two float32 arguments.

## Highlights

- **`CCALL2FF`**: `hypotf(x, y)` and `copysignf(x, y)` execute in J4 through
  libffi with `ffi_type_float` arguments and return.
- `xmm0`/`xmm1` order is checked with non-commutative `copysignf`.
- Same-Proto integer arguments fall back through the entry float guards.
- `tests/ffi/37_trace_ccall2ff.lua`.

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

- Mixed/float32/double signatures beyond the covered forms, varargs and
  aggregates remain CALL exits.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
