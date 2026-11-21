# Luar 0.58 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
three-double native C calls.

## Highlights

- **`CCALL3DD`**: `fma(x, y, z)` executes inside J4 with `xmm0`/`xmm1`/
  `xmm2`, guarded by function identity and three float argument tags.
- No new IR field: argument 2 and 3 slots pack into the existing `ival`.
- MOVE/LOADK float arguments, LOADK constants, z-position ordering,
  same-Proto integer fallback and function-identity rollback are tested.
- `tests/ffi/33_trace_ccall3dd.lua` and a new J4 benchmark.

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

- `OP_LOADF` constants are not yet recorded; LOADK float constants are.
- Four or more arguments, mixed integer/double layouts beyond the existing
  two-argument forms, float32, varargs and aggregates remain CALL exits.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
