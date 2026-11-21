# Luar 0.56 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
mixed double/int native C calls.

## Highlights

- **`CCALL2DI`**: `ldexp(x, e)` executes inside J4 when `x` is a float
  MOVE/LOADK and `e` is an integer MOVE/LOADK/LOADI.
- Argument order is enforced (`xmm0` double, `rsi` integer) and guarded.
- Same-Proto integer-first calls fall back without reaching the op.
- `tests/ffi/31_trace_ccall2di.lua` and a new J4 benchmark.

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

- Mixed calls cover `(double, int) -> double` only.
- `(int, double)`, multiple integers, float32, varargs and aggregates remain
  CALL exits.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
