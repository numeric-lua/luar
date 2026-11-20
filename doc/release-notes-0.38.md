# Luar 0.38 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
performance evidence for the FFI trace subset and error coverage for
continuations.

## Highlights

- **J4 FFI benchmarks**: `j4/ffi_cdata_read` and `j4/ffi_cdata_copy` exercise
  cdata integer/double reads and double copies through J4.
- **Erroring CALL**: a partial trace/continuation test now verifies that an
  error thrown by the interpreter-performed call unwinds cleanly and does not
  run the suffix continuation.
- The benchmarks skip cleanly when FFI or J4 is unavailable.

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

- `B == 0` calls and yield continuations remain unsupported.
- Arbitrary pointer arithmetic falls back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
