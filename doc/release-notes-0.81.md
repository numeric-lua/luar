# Luar 0.81 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds string table-field reads.

## Highlights

- `t.x` on a string field lowers to `TGETKS` when the live record-time field
  is a string; the helper copies the rooted `TString` into the stack slot.
- Integer/float fields keep their existing ops.
- `tests/jit/86_trace_string_field_read.lua` covers constructor reads,
  forced GC and the `alloc=0` path.

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

## Known limitations

- String reads are data movement only; string concatenation/equality remain
  interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
