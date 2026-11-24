# Luar 0.225 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes generic table-path key handling and extends it to negative keys.

## Fixes

- Sparse nil clears now pass the actual Lua key to `luaH_setint`, not the
  zero-based loop offset.  A range such as `t[1000..1200]` clears exactly
  that range and leaves `t[999]`/`t[1201]` untouched.
- Generic table copies, fills and nil clears accept negative and hash
  integer keys.  A separate FORLOOP endpoint helper avoids the positive-key
  restriction while retaining overflow checks and a `1 << 22` iteration
  cap.
- Numeric constant fills with a closure-upvalue destination
  (`U[i] = 7`) take the generic fill path instead of staying interpreted.

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

New test:

- `tests/jit/220_table_generic_keys.lua`

## Known limitations

- Generic paths use per-element `luaH_getint`/`luaH_setint` calls and are
  slower than the numeric fast paths.
- Zero still cannot be used as a generic run endpoint; the loop helper
  rejects a zero step but accepts negative indexes.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
