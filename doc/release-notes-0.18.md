# Luar 0.18 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends FFI-in-trace from arrays to scalar struct/union fields.

## Highlights

- **Struct field reads**: `CGETFIELD` reads signed integer and `float`/
  `double` fields from cdata structs/unions.  Bitfields, pointer fields,
  nested aggregates and unsupported types fall back.
- Field offsets are resolved at record time; the helper validates cdata
  identity, aggregate kind, offset/size bounds and reads without allocation.
- `tests/ffi/22_trace_cdata.lua` covers `struct { int x; double y; }` with
  both integer and double accumulations.
- Arrays and scalar fields now share the cdata read/write trace matrix.

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

- Struct fields are read-only in traces; struct writes and nested fields
  fall back.
- C calls and complex cdata values remain interpreter-side.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
