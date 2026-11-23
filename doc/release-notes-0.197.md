# Luar 0.197 release notes

This release is a correctness and JIT-coverage pass over the 0.196 runtime.

## JIT

* J2 now compiles constant-index array reads such as `t[1]` (`OP_GETI`),
  including the fused `acc = acc + t[k]` shape.  The generated code uses
  Lua 5.5's split array representation, guards `1 <= k <= #array` and the
  element tag, and exits at the `GETI` bytecode if the table shape or value
  type changes.  The existing `jit.opt.start("table=1")` opt-in enables the
  generic table tier as before.
* J2 now compiles numeric `and`/`or` conditions (`OP_TEST`/`OP_TESTSET`).
  The tested register is still guarded at loop entry, so a later call with
  `false` or `nil` goes back to the interpreter.
* Exit snapshots keep the collectable bit on every restored tag.  Before
  this release a table or string live in a register at a J2/J4 side exit
  could be restored as a non-collectable value, causing a spurious
  "attempt to index a table value" or a VM type confusion.

## FFI

* Struct/union metatypes dispatch `__eq`, `__lt`, `__le` and `__len`,
  choosing the handler from either operand as LuaJIT does.
* `p - n` now subtracts instead of adding.  Arithmetic on `void *` and
  incomplete object types is rejected, and pointer differences return a
  Lua integer as in LuaJIT.

## Tests

`make test`, `make WITH_FFI=1 test` and `make JIT=0 test` are green; the
ASan/UBSan pass is green for the FFI build.  New tests
cover pointer arithmetic (`tests/ffi/106_pointer_arith.lua`),
pointer-difference values (`107_pointer_diff.lua`), metatype relational and
length methods (`108_metatype_relational.lua`), J2 constant-index reads
(`tests/jit/144_geti_loop.lua`) and J2 `and`/`or` loops
(`tests/jit/145_and_or.lua`).  The existing snapshot self-test in
`tests/jit/62_exit_spill.lua` now checks heap-value tags.
