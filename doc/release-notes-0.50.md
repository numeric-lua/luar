# Luar 0.50 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release starts
the native C-call phase and hardens cdata pointer ownership.

## Highlights

- **Native zero-argument C calls**: a cdata C function with no parameters and
  an integer return (`ffi.C.getpid`) executes inside J4 through
  `CCALL0I`/libffi, without a Lua CALL frame.
- **Pointer ownership hardening**: all pointer-field tests retain their
  owning cdata arrays; `doc/ffi.md` documents the rule and
  `tests/ffi/24_pointer_lifetime.lua` verifies it across forced GC.
- The earlier FFI suite GC crash was traced to unretained temporary arrays in
  tests, not to trace codegen.

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

- Native C calls currently cover only zero-argument integer-returning
  functions; arguments, float returns, varargs and struct returns remain
  interpreter-side or CALL exits.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
