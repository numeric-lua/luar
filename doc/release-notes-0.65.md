# Luar 0.65 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends trace allocation to sized integer array constructors.

## Highlights

- **`{i}` constructors**: `NEWTABLE` records its array/hash hints, `SETLIST`
  is lowered to `TSETI` writes, and the table is rooted before any resize.
- `alloc=1` is still required; without it the loop stays interpreted.
- `tests/jit/76_trace_tnew_setlist.lua` covers allocation, GC and `alloc=0`.
- Fixed a vABC precheck abort for NEWTABLE/SETLIST and the TSETI ABI.

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

- Constructor sizes in the `k`/EXTRAARG form, string/hash fields, `SETLIST`
  with variable top, closures/strings and `ffi.new` remain interpreter-side.
- Allocated tables are not sunk.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
