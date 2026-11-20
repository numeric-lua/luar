# Luar 0.28 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
folds three-level scalar struct-field writes into one FFI trace op.

## Highlights

- **Three-level field writes**: `a.b.c.x = i` and `a.b.c.y = 2.5` are
  resolved at record time and executed as one `CSETFIELD`/`CSETFIELDK` with
  the combined offset.
- The recorder skips the two intermediate `GETFIELD` opcodes.
- `tests/ffi/22_trace_cdata.lua` covers three-level reads and writes.

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

- Deeper than three levels and pointer fields fall back.
- `B == 0` calls and error/yield continuations remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
