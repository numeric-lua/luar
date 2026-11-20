# Luar 0.52 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends native C calls to integer arguments converted to double parameters.

## Highlights

- **`CCALL1DI`**: calls such as `sqrt(i)` execute inside J4 through libffi;
  the integer argument is converted to a double parameter, and the double
  return feeds the float specialization.
- `tests/ffi/26_trace_ccall1.lua` checks the result against `math.sqrt` and
  asserts no CALL exit.
- Runtime function identity and integer-argument guards remain in place.

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

- Native C calls cover zero args, one integer arg with integer return, and
  one integer arg converted to double with double return.
- General float arguments, multiple arguments, varargs and aggregates remain
  CALL exits or interpreter-side.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
