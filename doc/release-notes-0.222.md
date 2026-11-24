# Luar 0.222 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds zero-length/flexible array support in cdef and extends table nil
clears and upvalue-table destinations.

## Highlights

- `T[0]` is accepted as a zero-sized array and as a struct member.
  `ffi.sizeof`/`ffi.new` report 0; indexing is bounds-checked rather than
  reproducing LuaJIT's unchecked access.
- A final `T[]` struct member is treated as a C99 flexible array member
  with zero size; a standalone `T[]` stays an incomplete type, and a
  flexible or `[?]` member anywhere else is still rejected.
- `t[i] = nil` now clears sparse runs: array slots are cleared directly and
  hash entries outside the array part are removed with `luaH_setint`.
  Unrelated hash keys survive.
- Table copies and nil clears can target a closure-upvalue table
  (`U[i] = src[i]`, `U[i] = nil`); the destination is re-read from the
  closure each entry.

## Correctness notes

- The sparse nil clear is bounded to `1 << 22` keys and requires a
  metatable-free destination.
- A zero-length array has no addressable elements; `a[0]` raises an
  out-of-bounds error.  This is a deliberate safety deviation from
  LuaJIT's unchecked access.
- The zero-length array error for a negative size is now
  `array size must be non-negative`.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make test-shared
make test-dist
tools/upstream-check.sh
```

New tests:

- `tests/ffi/186_zero_length_arrays.lua`
- `tests/ffi/187_flexible_array_members.lua`
- `tests/jit/213_table_nil_sparse.lua`
- `tests/jit/214_table_upval_store.lua`

## Known limitations

- A flexible `[]` member is only accepted in the final struct position;
  `[?]` members remain unsupported.
- Zero-length arrays cannot be indexed even at 0.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
