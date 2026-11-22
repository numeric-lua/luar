# Luar 0.125 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
hardens table-upvalue writes against `__newindex`.

## Highlights

- Every `TSETUP*` helper now checks the table metatable and rejects
  `__newindex`, so the interpreter handles the assignment.
- `tests/jit/116_trace_settabup_newindex.lua` confirms `__newindex` runs
  exactly once for a new key and never sees a raw-set bypass.

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

- Metatable-protected writes stay interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
