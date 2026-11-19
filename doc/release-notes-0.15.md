# Luar 0.15 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the FFI-in-trace subset from integer to floating cdata arrays.

## Highlights

- **Double/float cdata array reads**: `ffi.new("double[?]", n)` reads are
  recorded as `CGETF`; integer-driven accumulation is floatified and executed
  through `FADD`.
- The cdata identity is guarded, the key must be integer, and bounds/type
  checks stay in a no-allocation helper.
- `tests/ffi/22_trace_cdata.lua` now verifies both `int[?]` and `double[?]`
  loops and their `ir_executed` accounting.
- Unsupported shapes (structs, function pointers, cdata writes) still fall
  back to the interpreter.

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

- Only cdata integer/float array/pointer reads are specialized.
- No cdata writes, struct fields or C calls in traces yet.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
