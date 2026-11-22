# Luar 0.196 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
removes two long-standing JIT availability gaps, folds constant field reads,
caches FFI call interfaces, and clears tables in place.

## Highlights

- A rejected hot loop no longer disables its siblings.  J2 remembers each
  attempted backedge separately, so a function with several loops (for
  example Mandelbrot's outer `for` plus inner `while`) can still compile the
  loop that is actually compilable.  `numeric/mandel` measured 13.5 ms best
  CPU time instead of 20.9 ms on the development host.
- `jit.flush()` now resets the per-Proto hot counter using the existing
  epoch field.  Previously a loop that had ever compiled stayed blacklisted
  after a flush; now it becomes hot and recompiles, including a second
  flush/recompile cycle.
- J2 folds a constant string-key field read (`s = s + t.x`) when the table
  is a local register and the loop cannot write a table.  A per-entry value
  guard compares the live field's exact representation (integer or double
  bits); changing or removing the field invalidates the native variant and
  resumes the interpreter.  Missing fields and `__index` are never folded.
- Scalar libffi call interfaces are cached per interned function CType and
  reused by ordinary `ffi.C`/`ffi.load` calls and by J4's native call
  helpers.  This removes a full `ffi_prep_cif` from every call.
- `table.clear` clears the split array tag/value storage and the hash nodes
  directly, preserving capacity and without metamethod dispatch.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make dynasm-check
tools/upstream-check.sh
```

New tests:

- `tests/jit/141_multi_loop_retry.lua`
- `tests/jit/142_flush_recompile.lua`
- `tests/jit/143_field_read.lua`
- `tests/ffi/105_scalar_call_cache.lua`
- `tests/compat/table_clear.lua`

## Known limitations

- J2 remains a loop-level compiler; calls, generic `__index`/`__newindex`,
  aggregate values and non-whitelisted opcodes stay on the interpreter or
  the opt-in J4 tier.
- A folded field loop only supports a table in a register and one field per
  loop body.  It is invalidated, not re-specialised, when the field value
  changes; the next `jit.flush()` can compile the new value.
- The FFI call cache covers fixed scalar signatures (up to eight arguments)
  whose arguments and return are integers, floats, enums, booleans or
  pointers.  Aggregate and variadic signatures keep the per-call libffi
  preparation path.
- `table.clear` keeps capacity; it does not shrink a large table.
