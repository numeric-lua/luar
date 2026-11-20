# Luar 0.53 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends native C calls to a double argument and double return.

## Highlights

- **`CCALL1DD`**: `fabs(x)` executes inside J4 through libffi when `x` is a
  float loaded by `MOVE` or a float `LOADK` constant.
- Runtime `GUARD FLOAT` protects the argument; cdata-element/`CGETF`
  arguments are deliberately excluded and fall back without state errors.
- Same-Proto int↔float fallback is tested in both directions.
- `tests/ffi/27_trace_ccall1dd.lua` and `j4/ffi_ccall1dd`.

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

- Double arguments must be `MOVE`/`LOADK` floats; cdata-element arguments
  remain unsupported.
- Multiple arguments, varargs, float32 and struct returns remain CALL exits.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
