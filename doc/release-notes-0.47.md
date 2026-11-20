# Luar 0.47 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
hardens cdata pointer ownership and generalizes pointer-offset writes.

## Highlights

- **Pointer lifetime contract**: a cdata pointer does not keep an owned array
  or struct alive.  `doc/ffi.md` now documents this and
  `tests/ffi/24_pointer_lifetime.lua` exercises the retained-owner case
  across forced GC.
- **Root cause**: the previous multi-step pointer-write crash was a test that
  passed a temporary `ffi.new` array into a pointer field; GC later freed it.
  It was not a trace miscompile.  The test now retains both owners.
- **Arbitrary constant pointer-offset writes**: `(p + i + 1 + 1 + 1 + 1)[0]`
  scans all `ADDI`/`MMBINI` steps and emits one pointer-field write.

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

- `B == 0` calls remain unsupported in prefixes.
- Pointer arithmetic with variable/runtime offsets falls back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
