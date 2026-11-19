# Luar 0.16 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the FFI-in-trace subset to integer cdata array writes.

## Highlights

- **Cdata integer writes**: `CSETI` stores integer values into `int[?]`,
  pointer arrays and other 1/2/4/8-byte integer element types, with bounds,
  null-pointer and type validation in a no-allocation helper.
- Combined read/write loops such as `a[i] = i; s = s + a[i]` stay in J4.
- `tests/ffi/22_trace_cdata.lua` covers integer reads, integer writes and
  double reads.
- Unsupported cdata writes (structs, floats, foreign userdata) fall back.

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

- No float cdata writes, struct fields or C calls in traces yet.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
