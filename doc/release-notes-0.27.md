# Luar 0.27 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends native continuations and nested field access.

## Highlights

- **Call-result continuations**: the post-CALL suffix can consume the value
  returned by the interpreter-performed call; the recorder guards the result
  slot and the suffix runs natively.
- **Three-level field reads**: `a.b.c.x` is folded into one `CGETFIELD` with
  the combined offset.
- `tests/jit/69_call_cont.lua` now covers a result-consuming suffix.
- `tests/ffi/22_trace_cdata.lua` covers the three-level read.

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

- `B == 0` calls and error/yield continuations remain unsupported.
- Three-level writes and pointer fields still fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
