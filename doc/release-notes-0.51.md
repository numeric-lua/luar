# Luar 0.51 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends native C calls to one integer argument.

## Highlights

- **Native one-argument C calls**: `abs(i)` executes inside J4 through
  `CCALL1I`/libffi, with a runtime integer guard on the argument and a cdata
  identity guard on the function.
- Signed and unsigned declared integer argument/return sizes are handled
  from the CType; no Lua CALL frame is created.
- `tests/ffi/26_trace_ccall1.lua` covers positive and negative arguments.
- `j4/ffi_ccall0` benchmarks the zero-argument path.

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

- Native C calls currently cover zero arguments and one integer argument with
  integer returns.
- Float arguments/returns, varargs and struct returns remain CALL exits or
  interpreter-side.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
