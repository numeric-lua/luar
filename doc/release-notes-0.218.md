# Luar 0.218 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the default J2 table-store and two-array reduction coverage and
accepts adjacent string literals in cdef `sizeof`.

## Highlights

- Raw table element copies `dst[i] = src[i]` run natively, including
  in-place copies and a destination that has to grow to the source range.
- Raw table floor-division/modulo stores `dst[i] = src[i] // k` and `% k`
  support constant, register and upvalue scalars.  Negative operands use
  Lua's floor rules and integer wrap; float elements promote.  An integer
  zero divisor declines before writing so the interpreter raises the exact
  error, while a float zero follows the non-raising path.
- Two-array reductions now include `s = s +|- a[i] // b[i]` and
  `s = s +|- a[i] % b[i]` over raw tables and bounded cdata arrays.
- `t[i] = nil` clears a run inside a raw table's existing array part in one
  C pass.  Partial, descending and strided ranges are supported; hash keys
  and entries outside the run are preserved.  A metatable or a run past
  the array part falls back to the interpreter.
- `ffi.cdef` concatenates adjacent string literals before sizing them, so
  `sizeof("a" "bc")` works in static assertions, enums and array bounds.

## Correctness notes

- Integer zero divisors are detected before any array store; the helper
  returns to the interpreter, which raises `n // 0` or `n % 0` at the
  correct element.
- Float floor division and modulo keep IEEE behavior (`inf`/`NaN`) instead
  of raising.
- The nil-clear helper only writes slots already inside the destination's
  array part and never invokes `__newindex`, matching raw interpreter
  stores for an absent-key nil assignment.

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

- `tests/ffi/177_sizeof_adjacent.lua`
- `tests/ffi/178_j2_cdata_pair_idiv.lua`
- `tests/jit/199_table_copy_store.lua`
- `tests/jit/200_table_idiv_store.lua`
- `tests/jit/201_pair_idiv.lua`
- `tests/jit/202_table_nil_store.lua`

## Known limitations

- Nil-range clears stay inside the existing array part; sparse/hash keys
  and runs past `asize` use the interpreter.
- Raw-table store and copy helpers cap destination growth at `1 << 22`
  keys.
- These are J2 loop-level intrinsics; unsupported shapes fall back to the
  interpreter or the existing J4 path.
