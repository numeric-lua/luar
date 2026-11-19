# Luar 0.14 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
starts FFI-in-trace specialization with a deliberately narrow, safe subset.

## Highlights

- **Cdata integer array reads**: J4 records `CGETI` for `cdata` arrays and
  pointers whose element is a 1/2/4/8-byte integer.  The cdata identity is
  guarded, the key must be an integer, and bounds/type checks run in a
  no-allocation helper.
- **Safe fallback**: `double` arrays, structs, unions, function pointers,
  foreign userdata, null pointers and out-of-bounds accesses stay on the
  interpreter.
- `tests/ffi/22_trace_cdata.lua` exercises a traced `int[?]` sum and a
  double-array fallback.
- J4/FFI suites remain green in both `WITH_FFI=0` and `WITH_FFI=1` builds.

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

- Only integer cdata array/pointer element reads are specialized.
- No cdata writes, struct fields, float cdata or C calls in traces yet.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
