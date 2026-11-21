# Luar 0.80 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds float table-field reads.

## Highlights

- `t.x` on a float field lowers to `TGETKF` when the table's live field is
  float at record time; integer fields keep `TGETK`.
- The helper uses `luaH_getstr` and writes a Lua float; no allocation.
- `tests/jit/85_trace_float_field_read.lua` covers a local constructor and
  the `alloc=0` interpreter-constructor path.

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

- Field reads select int/float from the live record-time value; string or
  mixed-type fields fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
