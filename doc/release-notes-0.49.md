# Luar 0.49 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
closes the `B == 0` CALL question with an explicit fallback contract.

## Highlights

- **`B == 0` CALLs are intentionally interpreter-side**: the traced prefix
  rejects multret call setup because `L->top` is not maintained by native
  traces.
- `tests/jit/71_call_b0.lua` verifies correctness through the fallback.
- This is a documented boundary, not a silent miscompile.

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

- `B == 0` multret calls are never traced.
- Pointer arithmetic beyond the supported runtime/constant folds falls back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
