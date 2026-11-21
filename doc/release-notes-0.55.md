# Luar 0.55 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
two-double native C calls and float-expression arguments.

## Highlights

- **`CCALL2DD`**: `fmin(x, y)`, `fmax`, and `fmod` execute inside J4 when
  both arguments come from float MOVEs/LOADKs, with `xmm0`/`xmm1` order
  covered by a non-commutative `fmod` test.
- **Float expressions as `CCALL1DD` arguments**: `fabs(d[i] * 0.5)`,
  `fabs(d[i] / 2)`, and `fabs(d[i] + e[i])` now trace, including pointer
  fields and scalar double fields.
- Runtime float guards remain after the expression/read, so integer or
  table fallbacks never reach the double call op.
- `tests/ffi/29_trace_ccall2dd.lua` and
  `tests/ffi/30_trace_ccall1dd_float_arith.lua`; new J4 benchmarks.

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

GitHub Actions also runs the macOS shared/install/module job.

## Known limitations

- Two-argument coverage is double/double with MOVE/LOADK arguments.
- Mixed integer/double signatures, varargs, float32 and aggregates remain
  CALL exits.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
