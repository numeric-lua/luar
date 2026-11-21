# Luar 0.94 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds constant boolean table-field writes.

## Highlights

- `{ x = true, y = false }` lowers to `TSETKB`; the key is interned and the
  boolean tag is set directly.
- `tests/jit/97_trace_boolean_field.lua` covers both values, forced GC and
  the `alloc=0` constructor path.

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

- Struct/pointer cdata upvalues remain interpreter-side; the userdata tag
  is restricted to cdata arrays.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
