# Changelog

All notable changes to Luar are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow semver for
the runtime, while the Lua language surface follows upstream Lua.

## [Unreleased]

## [0.207.0] — 2026-11-24

### Added

- J2 stores scaled bounded cdata arrays with `a[i] = b[i] +|-|*|/ v`,
  including register, immediate and constant scalar operands and the
  commutative add/multiply order.  The helper performs Lua's
  int-wrap/float-promotion arithmetic before the FFI element-store
  conversion and preserves the bytecode result temporary.
- J2 divides two bounded cdata arrays with `a[i] = b[i] / c[i]`, joining
  the existing add/subtract/multiply pair stores.  Division always
  produces a Lua float before the destination store conversion.
- J2 fills 8-byte integer cdata arrays with `a[i] = v|i|constant`
  (`int64_t` and `uint64_t` elements), including the out-of-range
  double-to-integer sentinel and unsigned wraparound.  Copy/update/pair
  forms stay interpreted because an 8-byte element read yields cdata, not
  a Lua number, and its temporary must remain observable.

### Fixed

- `string.format("%q", cdata)` now formats pointers, arrays, aggregates
  and CType objects through `tostring`, matching LuaJIT.  Previously only
  numeric cdata reached the wrapper and the others raised "value has no
  literal form".

### Performance

- Parsed C type names are cached per Lua state (bounded at 1024 distinct
  spellings) for `ffi.new`/`ffi.cast`/`ffi.sizeof`/`ffi.typeof`.  Cached
  lookups avoid reparsing the declaration on every call; successful
  forward-struct and later-typedef definitions still work.  A 200k-call
  lookup check measured `ffi.sizeof` at 62 ms cached versus 945 ms with a
  new spelling each time, and `ffi.typeof` at 64 versus 954 ms.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tools/upstream-check.sh` and
`make dynasm-check` are green.  New coverage:
`tests/ffi/135_j2_cdata_scalar_store.lua`,
`tests/ffi/136_j2_cdata_pair_div.lua`,
`tests/ffi/137_j2_cdata_i64_fill.lua`,
`tests/ffi/138_ctype_string_cache.lua` and
`tests/ffi/139_format_q_cdata.lua`.

## [0.206.0] — 2026-11-23

### Added

- J2 runs bounded cdata array stores in the default tier:
  `a[i] = v`, `a[i] = i` and `a[i] = constant`, for 1/2/4-byte integer and
  4/8-byte float elements.  A bounds-checked C helper follows the
  interpreter's FORLOOP key sequence for any nonzero integer step, applies
  the FFI element-store conversion (including LuaJIT's out-of-range
  double-to-integer sentinel) and keeps pointer sources on the interpreter.
- J2 copies bounded cdata arrays with `a[i] = b[i]`, converting between
  integer widths and float/double element types at each store while
  preserving the source-element temporary.
- J2 updates bounded cdata arrays in place with
  `a[i] = a[i] +|-|*|/ v`, including register, immediate and constant
  scalar operands, with Lua's int-wrap/float-promotion rules followed by
  the C store conversion.
- J2 stores two-source cdata results with `a[i] = b[i] +|-|* c[i]` over
  bounded arrays, converting the computed value to the destination element
  type and preserving both source temporaries.
- `ffi.cast` accepts a Lua string for any pointer or function-pointer type,
  matching LuaJIT; `ffi.new` keeps its stricter initializer rules.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tools/upstream-check.sh` and
`make dynasm-check` are green.  New coverage:
`tests/ffi/130_j2_cdata_store.lua`,
`tests/ffi/131_j2_cdata_copy.lua`,
`tests/ffi/132_j2_cdata_update.lua`,
`tests/ffi/133_cast_string_pointer.lua` and
`tests/ffi/134_j2_cdata_pair_store.lua`.

## [0.205.0] — 2026-11-23

### Added

- J2 runs two-source bounded cdata reductions in the default tier:
  `s = s + a[i] + b[i]`, the subtract form, and
  `s = s + a[i] * b[i]` (including `a[i] * a[i]`).  A bounds-checked C
  helper follows the interpreter's FORLOOP sequence for any nonzero integer
  step, preserves per-operation int wrap/float promotion, the observed
  element and product/result temporaries and the control slots, and is
  visible through `jit.stats().tint_hits`.  Pointer sources and mixed
  table/cdata pairs stay on the interpreter.

### Fixed

- The J2 table-fill intrinsic grew the array part with a zero-sized hash
  part.  Filling a table that already had a string, negative or high
  integer key silently dropped those entries.  The fill now uses
  `luaH_resizearray` and preserves them.
- J2 leaf-call intrinsics snapshotted extra arguments once.  A loop such as
  `s = s + f(i, i)` or `s = s + f(i, s)` therefore reused the first
  iteration's argument and returned a different result once hot.  The fast
  path now requires extra argument registers to be loop-invariant;
  constant arguments still compile.
- J4's integer `ADDK`/`SUBK`/`MULK` used x86 32-bit immediate forms, so
  constants such as `2^32` and `maxint` were truncated.  They now use the
  full-width scratch-register helpers, matching the interpreter.
- `ffi.new` of a union from a table ignored a named member other than the
  first, silently zeroing the union.  Named members are now selected by
  name, while positional entries still initialize only the first member.
- An aggregate lvalue initializes a `void *` in `ffi.new` and pointer
  assignments, as in LuaJIT; mismatched object pointers and numeric scalars
  remain rejected.
- `ffi.metatype` rejects a second registration for the same type even when
  the original table is passed again, matching LuaJIT and the documented
  one-metatype rule.  `__len`/`__pairs` on a type without a metatype now
  raise the intended cdata error instead of indexing a missing registry
  entry.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tools/upstream-check.sh` and
`make dynasm-check` are green.  New coverage:
`tests/ffi/125_union_named_init.lua`,
`tests/ffi/126_void_pointer_aggregate.lua`,
`tests/ffi/127_metatype_single_registration.lua`,
`tests/ffi/128_no_metatype_dispatch.lua`,
`tests/ffi/129_j2_cdata_pair.lua`,
`tests/jit/173_fill_preserves_hash.lua`,
`tests/jit/174_leaf_arg_variation.lua` and
`tests/jit/175_j4_int64_k_constants.lua`.

## [0.204.0] — 2026-11-23

### Added

- J2 runs bounded cdata array reductions in the default tier:
  `for i = 0, n - 1 do s = s + a[i] end` for 1/2/4-byte integer and
  4/8-byte float elements.  A bounds-checked C helper follows the
  interpreter's FORLOOP count/index sequence, preserves int64 wrap,
  unsigned widening, int-to-float promotion and the post-loop element
  temporary, and is observed through `jit.stats().tint_hits`.  Pointer
  sources have no length and stay on the interpreter.
- Function symbols are cached per `ffi.C`/`ffi.load` namespace.  Repeated
  `ffi.C.fn` lookups return the same cdata object as LuaJIT (`rawequal` is
  true), avoid repeated `dlsym`/userdata allocation and keep the library
  alive for the cached pointer.  Variables and constants remain snapshots.

### Fixed

- Out-of-range double-to-integer conversions are defined and match
  LuaJIT's observable x86-64 behaviour instead of C undefined behaviour:
  signed overflow and NaN/infinity use the `INT64_MIN` sentinel,
  `[2^63, 2^64)` converts exactly, and `2^64` wraps to zero.  This covers
  `ffi.new`/`ffi.cast`, element and field stores, cdata arithmetic and
  numeric comparisons.
- J4 no longer records 8-byte integer struct/union fields.  A `long` or
  `int64_t` member read yields cdata int64, not a Lua integer; treating it
  as an integer could silently return a pointer-like value for
  `x = x + s.z` once the loop became hot.  Such loops now stay on the
  interpreter, while narrower integer fields keep tracing.
- `ffi.new` pointer initializers follow LuaJIT's type rules: numbers,
  numeric cdata and incompatible pointers are rejected; `void *`
  interoperates with object pointers, arrays decay to a matching pointer,
  and an aggregate initializes a pointer to its exact type.  The existing
  `char *` string convenience is preserved and `ffi.cast` stays
  permissive.

### Performance

- A 2M-element `double[?]` cdata sum measured 239 ms interpreted versus
  2.2 ms with the new J2 intrinsic; the `int[?]` form measured 249 ms
  versus 2.1 ms.  Two million repeated `ffi.C.abs` lookups dropped from
  960 ms to 476 ms with symbol caching.  These are development-host
  microchecks, not release gates.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared`, `make test-dist`,
`tests/upstream/run.sh` and `tools/upstream-check.sh` are green.  New
coverage: `tests/ffi/120_float_int_conversion.lua`,
`tests/ffi/121_j2_cdata_sum.lua`,
`tests/ffi/122_int64_field_no_trace.lua`,
`tests/ffi/123_new_pointer_strict.lua` and
`tests/ffi/124_symbol_cache.lua`.

## [0.203.0] — 2026-11-23

### Added

- FFI arrays decay to pointers for arithmetic and address comparison, as in
  C and LuaJIT: `a + 1`, `1 + a`, `a - 1`, `a - a`, `a == p` and compatible
  array/pointer ordering now work and produce pointer or integer results.
  Pointer ordering also requires compatible base types, and mixing function
  pointers with object pointers or arrays raises.
- J2 sums three raw arrays from any mix of register locals and closure
  upvalues (`s = s + a[i] + b[i] + c[i]`), re-resolving each upvalue at
  every entry and preserving the exact int-wrap/float-promotion order.
- J2 runs scaled raw-array reductions (`s = s + a[i] * k`) where the scalar
  is a numeric constant, a register local/parameter or a closure upvalue.
  The element and product registers keep their post-loop values.
- J2 raw-array reductions now follow the interpreter's FORLOOP key sequence
  for any nonzero integer step, including descending loops.  The
  single-array sum, pair sum/difference, dot product, three-array sum and
  scaled sum intrinsics share overflow- and bounds-checked strided walks.

### Fixed

- The method-add intrinsic used the GETFIELD key as the store key, so
  `function o:add(x) self.w = self.v + x end` accumulated into `self.v` and
  never updated `self.w`.  It now requires the same constant key for the
  read and write and a `self` receiver.
- The fast `while` peephole dropped an `i*i` temporary when the body also
  used `s = s * i`, `s = s - i` or `s = s / i`: the emitter selects one
  operation and only the square/linear forms write the temporary.  Such
  bodies fall back to the generic branch emitter.
- Generic bottom-tested `repeat` loops used a reusable local DynASM label
  for their exit.  On a negative-step body whose work precedes the
  increment, the exit label resolved incorrectly, leaving `eax` unset; the
  caller then re-ran the body one extra time (or looped).  The exit now
  uses the same epilogue label as ordinary `while` loops.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/119_array_decay.lua`,
`tests/jit/167_three_sum_upvalues.lua`,
`tests/jit/168_scaled_array_sum.lua`,
`tests/jit/169_strided_array_sum.lua`,
`tests/jit/170_method_distinct_field.lua`,
`tests/jit/171_while_temp_writes.lua` and
`tests/jit/172_repeat_negative_step.lua`.

## [0.202.0] — 2026-11-23

### Added

- J2 sums and subtracts two raw arrays in one C pass
  (`s = s + a[i] + b[i]` and `s = s + a[i] - b[i]`).  Register and closure
  upvalue arrays are supported, and the accumulator and every element or
  temporary register visible after the loop are updated with the interpreter
  value.  `jit.stats().tint_hits` observes the intrinsic.
- J2 computes dot products and sums of squares over raw arrays
  (`s = s + a[i] * b[i]` and `s = s + a[i] * a[i]`) with the interpreter's
  int64 wrap and mixed int/float promotion order.  The product temporary is
  preserved for code that reads it after the loop.
- FFI integer and length arguments accept numeric cdata: `ffi.new` VLA
  lengths, `ffi.sizeof`, `ffi.string`, `ffi.copy`, `ffi.fill`, `ffi.errno`
  and integer cdata array indices.  Float cdata is truncated for lengths but
  rejected as an array index, as in LuaJIT.

### Fixed

- The fast `while` peephole kept only the last step when the body wrote its
  induction variable more than once.  `while i < n do ...; i = i + 1;
  i = i + 1 end` ran with step 1 and silently produced a different result;
  such loops now fall back to the generic emitter/interpreter.
- `ffi.offsetof` reported packed bitfields relative to their first byte and
  returned a bit position for full-width bitfields.  It now matches LuaJIT:
  narrow packed fields are byte-relative, wider packed fields use the base
  type's storage unit, and a full-width bitfield is returned like a normal
  member.

### Tests

`make test`, `make WITH_FFI=1 test` and `make JIT=0 test` are green.  New
coverage: `tests/jit/164_fast_while_multi_inc.lua`,
`tests/jit/165_pair_array_sum.lua`, `tests/jit/166_dot_product.lua`,
`tests/ffi/117_offsetof_bitfields.lua` and
`tests/ffi/118_cdata_int_args.lua`.

## [0.201.0] — 2026-11-23

### Added

- J2 runs a `s = s + a[i] + b[i] + c[i]` intrinsic over three raw arrays.
  The intrinsic re-checks every table's array shape and each element tag, so
  short tables, metatables and non-numeric elements fall back without
  changing the visible accumulator.  `jit.stats().tint_hits` observes it.
- FFI metatypes dispatch `__add`, `__sub`, `__mul`, `__div`, `__idiv`,
  `__mod`, `__pow`, `__band`, `__bor`, `__bxor`, `__unm`, `__bnot` and
  `__concat`, matching LuaJIT's left-then-right handler selection and the
  duplicated operand pair for unary handlers.
- A metatype `__gc` is the default finalizer for its struct/union cdata.
  `ffi.gc(x, f)` replaces it and `ffi.gc(x, nil)` suppresses it, as in
  LuaJIT.
- Metatype `__tostring`, `__len` and `__pairs` apply to a pointer to the
  owning struct/union, as they already did for `__index`, `__newindex` and
  `__call`.

### Fixed

- J2 rejected only branch loops whose carried registers change type.  A
  straight-line body such as `t = u + i * 1.5; u = s - i` changed `u` from
  an integer to a float; the next native iteration read the double payload
  as an integer.  All non-specialised loops now require a single type per
  carried register and fall back to the interpreter otherwise.
- The fast `while` peephole compared a float limit as its raw 64-bit bit
  pattern.  A subnormal limit with bit pattern 100 ran the wrong number of
  iterations, and `while i < 100.0` could hang.  Float limits now use the
  generic branch emitter's exact mixed int/float comparison.
- J4 `ir_floatify` converted only the arithmetic chain rooted at the first
  float slot it saw.  Later independent `t = t * 1` or `u = s * -1` ops
  stayed integer MULK instructions on double operands.  A trace with an
  unconverted integer op on a float slot is now rejected.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tests/upstream/run.sh` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/jit/160_straight_type_change.lua`,
`tests/jit/161_three_table_sum.lua`,
`tests/jit/162_while_float_limit.lua`,
`tests/jit/163_ir_mixed_float.lua`,
`tests/ffi/114_metatype_arith.lua`,
`tests/ffi/115_metatype_gc.lua` and
`tests/ffi/116_metatype_pointer.lua`.

## [0.200.0] — 2026-11-23

### Added

- J2 sums a raw array captured in a closure upvalue (`local t = ...; ...
  s = s + t[i] ...`) with a table intrinsic that re-resolves the upvalue at
  every entry, so reassignment and metatable fallback keep working.
  Plain register-table sums keep their existing path.
- Raw array and keyed sums no longer stop at 4M remaining iterations.  The
  old cap made every backedge of a larger loop attempt and reject the
  intrinsic, which could run it slower than the interpreter.

### Fixed

- The fast `while`/`repeat` peephole no longer folds a body that writes its
  accumulator more than once.  `a = a + i*i; a = a*i` (and the sum/product,
  difference/product forms) matched both shapes; the emitter chose one and
  silently computed a different loop.  Such bodies now use the generic
  while decoder or the interpreter.
- A raw-table intrinsic that cannot handle the current iteration is dropped
  at that backedge instead of being retried forever.  A missing key late in
  a large keyed sum was quadratic; it now falls back to T0 after one
  attempt.
- FFI enums convert numerically as unsigned 32-bit values, matching LuaJIT:
  `ffi.new("enum E", -1)` still prints as `-1`, but `tonumber`, casts,
  arithmetic and comparisons use 4294967295.  Struct enum fields follow the
  same view.
- FFI integer power returns the `INT64_MAX` infinity sentinel for
  `0 ^ negative` instead of zero.  Unsigned cdata keeps its existing
  wrapping behaviour, as in LuaJIT.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test` and
`make WITH_FFI=1 test-asan` are green.  New coverage:
`tests/jit/157_upvalue_table_sum.lua`,
`tests/jit/158_while_multi_acc.lua`,
`tests/jit/159_table_sum_large.lua`,
`tests/ffi/112_enum_unsigned.lua` and
`tests/ffi/113_pow_zero_negative.lua`.

## [0.199.0] — 2026-11-23

### Added

- J2 decodes the loop's one raw array read (`GETI`/`GETTABLE`) anywhere in a
  branch body, not only as the first instruction.  `if cond then s = s + t[i]
  end` now runs natively while keeping the table/metatable/bounds guards and
  the exact exit PC.
- `jit.stats().tint_hits` reports successful raw-table intrinsic runs, so the
  table fill/sum fast paths are observable without timing.
- FFI integer arguments (lengths, `ffi.fill` byte, `ffi.errno`, array
  indices) use C conversion rules: finite Lua floats truncate towards zero
  and numeric strings are rejected.  NaN, infinity and out-of-range values
  raise instead of invoking an undefined C conversion.

### Fixed

- Branch-loop entry guards use the operand flavour for float `ADDK`/`SUBK`/
  `MULK`/`DIVK` and `UNM`.  These loops compiled but always failed their own
  integer guard, so float branch bodies silently stayed interpreted.
- Inlined leaf calls no longer turn a zero divisor into zero.  `leaf_eval()`
  now bails to the interpreter for `//`/`%` by zero, preserving the exact
  divide-by-zero error instead of finishing the loop with a wrong sum.
- Integer-accumulator raw table and keyed sums promote to float when the
  first float element or value is added, matching the interpreter's mixed
  arithmetic instead of falling back to the interpreter.

## [0.198.0] — 2026-11-23

### Added

- J2 executes compiled float-controlled `for` loops.  The VM recorded and
  compiled these backedges but never handed the next iteration to the
  generated code, so they ran on the interpreter despite appearing in
  `jit.stats().compiled`.  The float `FORLOOP` path now mirrors the integer
  one, including guard exits and `jit.flush()` invalidation.
- J2 compiles branch bodies (`if`/`else`, numeric `and`/`or`) inside
  float-controlled `for` loops.  Branch loops added integer entry guards for
  the control triplet regardless of `for_float`, so a float loop with an
  `if` always failed its own guard and silently stayed interpreted.

### Fixed

- A fused `acc = acc + t[const]` (`GETI`) no longer loses the bytecode
  result register.  `local x = t[1]; s = s + x` left `x` at its pre-loop
  value when the loop ran natively; the native path now materialises
  `R[A]`, and the fusion is rejected when the lookup result is also the
  accumulator.
- FFI metatype `__eq` is called for two references to the same struct or
  union cdata, matching LuaJIT.  Stock Lua short-circuits identical
  userdata; the FFI-aware equality hook now routes the pair through the
  handler, while `rawequal()` and cdata without an `__eq` handler keep raw
  identity.
- Pointer subtraction requires compatible base types, matching LuaJIT:
  same-size integer types (including enums) are compatible, while different
  struct tags, struct/union mixes, different element widths and function
  pointers are rejected.  Floating Lua offsets truncate towards zero, and
  NaN/infinite/out-of-range offsets behave as zero instead of an undefined
  integer conversion.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test` and
`make WITH_FFI=1 test-asan` are green.  New coverage:
`tests/jit/146_geti_result.lua`, `tests/jit/147_float_for_native.lua`,
`tests/jit/148_float_for_branch.lua`, `tests/ffi/109_pointer_compat.lua`
and `tests/ffi/110_metatype_eq_identity.lua`.

## [0.197.0] — 2026-11-23

### Added

- J2 compiles leading constant-index array reads (`t[1]`, `OP_GETI`) with
  the 5.5 split-array representation.  The native path guards the table,
  the `1..asize` bound and the element tag, includes the fused
  `acc = acc + t[k]` form, and exits at the `GETI` bytecode on a shape or
  type mismatch.  Enabled with the existing `jit.opt.start("table=1")`
  opt-in.
- J2 compiles numeric `and`/`or` truth tests (`OP_TEST`/`OP_TESTSET`).
  A number or table is always truthy, but the tested register keeps an
  entry type guard, so a later call with `false`/`nil` falls back to the
  interpreter instead of running the native truthiness assumption.
- FFI metatypes dispatch `__eq`, `__lt`, `__le` and `__len` for struct and
  union cdata.  A handler on either operand is used, matching LuaJIT;
  numeric cdata, pointers and arrays keep their built-in FFI semantics.

### Fixed

- Pointer arithmetic subtracts the offset for `p - n` (previously it added
  it), and `void *` or pointers to incomplete object types are rejected
  instead of being treated as byte pointers.
- Pointer subtraction returns a Lua integer count of elements rather than
  an `int64_t` cdata, matching LuaJIT and avoiding a cdata allocation.
- J4/J2 exit snapshots preserve the collectable tag bit.  Restoring a
  snapshot used `ttypetag()`, which strips that bit, so a live table (or
  string/function) slot came back as a non-collectable value and the
  interpreter subsequently rejected it as "not a table".  The bug could
  corrupt any side exit with a heap value live in a register.

## [0.196.0] — 2026-11-22

### Added

- J2 folds a constant string-key field read (`t.x`) when the table is a
  local register and the loop cannot mutate a table.  A per-entry value
  guard compares the live field, so a changed or removed field invalidates
  the native variant and returns to the interpreter; missing fields and
  `__index` stay on the interpreter.  A field loop now runs natively instead
  of merely being recorded.
- `jit.stats().ir_side_parent` reports the root-loop start that produced an
  interior side exit, so side-trace tests remain meaningful when another
  Proto has since compiled its own root.
- FFI caches the prepared libffi call interface for scalar signatures, both
  for ordinary `ffi.C`/`ffi.load` calls and for the J4 trace call helpers.
  Repeated calls no longer rebuild the `ffi_cif` each time.
- `table.clear` clears the array and hash parts in C, preserving capacity
  and never invoking `__index`/`__newindex`.

### Fixed

- A hot loop that J2 cannot compile no longer blacklists every other loop
  in the same Proto.  Rejected backedges are remembered individually, so an
  inner `while` or a sibling numeric loop still gets compiled (this is what
  makes `numeric/mandel` run natively).
- `jit.flush()` resets the per-Proto hot state through the existing epoch
  field.  A flushed loop becomes hot and compiles again instead of staying
  permanently disabled; stale code/intrinsic slots are discarded on the
  next compile.

### Performance

- `numeric/mandel` best CPU time fell from about 20.9 ms to 13.5 ms on the
  development host because the inner `while` loop now compiles.
- `table/new_clear` fell from about 10.3 ms to 3.5–4.2 ms in the same
  harness.
- J4 scalar C calls improved by roughly 25–35% (`ffi_ccall3dd` 30.8→22.5 ms,
  `ffi_ccall2dd` 29.1→20.6 ms, `ffi_faddi_call` 25.0→16.1 ms); the same
  cached interfaces are used by ordinary calls.

## [0.195.0] — 2026-11-22

### Added

- J2 compiles integer floor division and modulo (`//`, `%`, `IDIVK`, `MODK`)
  with Lua's round-towards-negative-infinity rule.  Division by zero and the
  `INT64_MIN // -1` / `%-1` special cases exit to the interpreter so errors
  and wrapping stay exact.
- J2 compiles constant-left shifts (`OP_SHLI`, e.g. `3 << i`), including
  negative counts and counts of 64 or more.
- J2 compiles numeric `for` loops whose control values are floats, with the
  interpreter's limit/step/index layout, IEEE addition and NaN-aware
  termination.

### Fixed

- J2 table paths use Lua 5.5's split array representation (inverted `Value`
  array plus a separate tag array) and the collectable table tag.  The
  generic opt-in table emitter previously failed its tag guard on every
  iteration; once it ran, the old 16-byte access would have corrupted
  neighbouring array slots.
- The fast `s = s + t[i]` emitter advances the loop index and materialises
  the `GETTABLE` result slot, so a local assigned from the element is correct
  after the loop.
- FFI `_Bool` conversion follows C: every non-zero scalar, including a
  fractional, infinite or NaN float, becomes 1, and `_Bool` bitfields store
  and read the canonical 0/1 form.
- `luar.version_num` now encodes 0.195.0 (19500); the numeric minor
  component had lagged behind the release string.

## [0.194.0] — 2026-11-22

### Added

- J2 compiles register-count integer shifts (`OP_SHL`/`OP_SHR`), including
  negative counts, counts of 64 or more, and multiple shifts in one loop.
- J2 compiles integer and float unary minus (`OP_UNM`).  Integer negation
  wraps at `INT64_MIN`; float negation flips the IEEE sign bit.

### Fixed

- A J2 guard exit from a mixed integer/float comparison resumes at the
  comparison's bytecode PC.  Decoding zeroed the stored PC, so such an exit
  restarted the function and repeated its side effects.
- The specialised repeat emitter is selected only when the body consists of
  the matched increment/accumulator operations and `MMBIN`/`NOP`
  placeholders.  Extra assignments or branches are no longer dropped.
- A 64-bit FFI bitfield that starts at a non-zero bit offset spans nine
  bytes; loading or storing it stays inside the field instead of copying a
  ninth byte through a `uint64_t` temporary.

## [0.193.0] — 2026-11-22

### Fixed

- J2 rejects loops whose registers change type between iterations, instead
  of reading a float payload as an integer (or vice versa) after a
  branch-dependent assignment.
- Fast `while`/`repeat` loops no longer cache a limit that is also the
  accumulator or induction variable.
- Fast numeric, table and inlined-`__index` paths materialise their
  intermediate result in the bytecode register, so a local holding `i*i`,
  `i*const`, a table element or a method result is correct after the loop.
- Packed zero-width bitfields advance to their base type's natural boundary
  even inside `__attribute__((packed))` or `#pragma pack`.
- `#pragma pack(n)` packs consecutive bitfields with the same tight rules
  as `pack(1)`; ordinary members and aggregate alignment remain capped by
  `n`.

## [0.192.0] — 2026-11-22

### Added

- J2 compiles integer `&`, `|`, `~`, unary `~` and constant `<<`/`>>`
  loop bodies, including the `BANDK`/`BORK`/`BXORK` and `SHRI` forms.

### Fixed

- J2 keeps full-width integer constants in `ADDK`/`SUBK`/`MULK` and the
  inlined `__index` multiplier; operands no longer truncate to 32 bits.
- J2 compares mixed integer/float operands exactly, falling back to the
  interpreter at the `2^53` boundary instead of comparing rounded doubles.
- Packed unions of bitfields are sized by the bytes their bits occupy,
  matching the C compiler and `_Alignof`.
- `table.new` rejects negative and `INT_MAX`-overflowing array/record
  sizes instead of ignoring or wrapping them.

## [0.191.0] — 2026-11-22

### Added

- J4 fuses int64 array shifts (`CSHLI64`/`CSHRI64`), unary minus
  (`CNEGI64`) and bitwise not (`CBNOTI64`) in addition to the existing
  add/multiply/divide/modulo/bitwise forms.

### Fixed

- Fused int64 operations keep full 64-bit constant operands; constants
  above `INT_MAX` or `2^53` no longer truncate or lose precision.
- Fused `//`/`%` on `uint64_t` arrays use unsigned semantics.
- Fused int64 add/multiply use wrap-defined unsigned arithmetic, and
  `INT64_MIN / -1` / `% -1` no longer raise `SIGFPE` or invoke C undefined
  behaviour.
- Sanitizer builds no longer fault on entry to JIT-compiled code when the
  compiler's `-fsanitize=undefined` includes the function-type check
  (recent Clang); generated-code entry points opt out of that check only.

## [0.26.0] — 2026-11-19

### Changed

- Side-chain fallback uses the memory-form exact exit state; the runtime
  rollback snapshot was removed from the side path.
- Documented why J4 needs no native register-spill stub.

## [0.25.0] — 2026-11-19

### Added

- Native post-CALL suffix continuation for C and Lua calls, with
  `ir_cont_compiled`/`ir_cont_runs` stats.
- `tests/jit/69_call_cont.lua`.

## [0.24.0] — 2026-11-19

### Added

- Two-level scalar struct-field writes folded into one `CSETFIELD`/
  `CSETFIELDK`.
- `tests/ffi/22_trace_cdata.lua` now covers `o.inner.x = i` and
  `o.inner.y = 2.5`.

## [0.23.0] — 2026-11-19

### Added

- Two-level scalar struct-field reads folded into one `CGETFIELD`.
- `tests/ffi/22_trace_cdata.lua` now covers `o.inner.x`.

## [0.22.0] — 2026-11-19

### Added

- CALL-exit prefixes for C functions and callable tables, not only Lua
  closures.
- `tests/jit/68_call_c_exit.lua`.

## [0.21.0] — 2026-11-19

### Added

- Constant `float`/`double` struct/union field writes in J4.

## [0.20.0] — 2026-11-19

### Added

- `CSETFIELDK`: constant integer struct/union field writes in J4.
- `tests/ffi/22_trace_cdata.lua` now covers constant field writes.

## [0.19.0] — 2026-11-19

### Added

- `CSETFIELD`: scalar signed integer and `float`/`double` struct/union field
  writes in J4.
- `tests/ffi/22_trace_cdata.lua` now covers struct field reads and writes.

## [0.18.0] — 2026-11-19

### Added

- `CGETFIELD`: scalar signed integer and `float`/`double` struct/union field
  reads in J4, with bitfield rejection and bounds checks.
- `tests/ffi/22_trace_cdata.lua` now covers struct int and double fields.

## [0.17.0] — 2026-11-19

### Added

- `CSETF`: float/double cdata array and pointer writes in J4.
- `tests/ffi/22_trace_cdata.lua` now covers double copies.

## [0.16.0] — 2026-11-19

### Added

- `CSETI`: integer cdata array/pointer writes in J4, with bounds/type checks
  and no allocation.
- `tests/ffi/22_trace_cdata.lua` now covers int reads, int writes and double
  reads.

## [0.15.0] — 2026-11-19

### Added

- `CGETF`: double/float cdata array and pointer reads in J4, with float
  accumulation through the existing float specialization.
- `tests/ffi/22_trace_cdata.lua` now covers both int and double arrays.

## [0.14.0] — 2026-11-19

### Added

- FFI-in-trace subset: integer cdata array/pointer elements read through
  `CGETI` with an identity guard and bounds/type validation.
- `tests/ffi/22_trace_cdata.lua`; unsupported cdata shapes fall back.

## [0.13.0] — 2026-11-19

### Added

- Side-segment hash insertion coverage (`tests/jit/67_side_hash_insert.lua`).
- `j4/hash_insert` benchmark from the 0.12 cycle.

## [0.12.0] — 2026-11-19

### Added

- Hash insertion/rehash through J4 `TSET` with an L-aware loop ABI.
- `tests/jit/66_hash_insert.lua` and the `j4/hash_insert` benchmark.

## [0.11.0] — 2026-11-19

### Added

- Existing-cell J4 side `TSET` on tables with a metatable, while empty cells
  still honor `__newindex`.
- macOS C-module compile/load/require check in CI.
- `tests/jit/65_side_meta_tset.lua`.

## [0.10.0] — 2026-11-19

### Added

- Existing-hash-cell integer `TSET` in J4 side segments, without allocation
  or metamethod fallback.
- `tests/jit/64_side_hash_tset.lua` covering hash writes and reads.

## [0.9.0] — 2026-11-19

### Added

- `TSET` in chained J4 side segments, using exact-PC fallback instead of
  replaying an already-mutated prefix.
- `tests/jit/63_side_tset.lua` covering a side that performs a table write.

## [0.8.0] — 2026-11-19

### Added

- Full-width (`LUAR_IR_MAX_SLOTS` = 64) exit snapshot/restore selftest.
- Growing-write/TSET fallback regression test.
- macOS shared-dylib link/run and staged-install/pkg-config checks in CI.

## [0.7.1] — 2026-11-19

### Changed

- Renamed all display branding from “LuaR” to **Luar**.  Executables,
  modules, C symbols and `LUAR_*` macros are unchanged.

## [0.7.0] — 2026-11-19

### Added

- J4 float subtraction (`FSUB`) and division by constant (`FDIVK`).
- `tests/jit/61_ir_float_arith.lua`.
- Depth-4 chained side traces and blocked no-progress tracking from the
  0.6 cycle.

## [0.6.0] — 2026-11-19

### Added

- J4 side chains now reach depth 4 (parent + three children), reusing exact
  failure PCs and parent state.
- Blocked/no-progress PC tracking for side chain children.
- `tests/jit/60_side_chain_depth.lua` covering a four-closure chain.

## [0.5.0] — 2026-11-19

### Added

- Chained J4 side traces: a parent side can compile and execute one child
  segment at its exact failure PC, with depth-2 tracking and reuse.
- `tests/jit/59_side_chain.lua` covering closure-identity chaining.
- Exact side-failure PCs and mixed int/float root correctness from the 0.5
  cycle.

### Changed

- J4 side compilation rejects unsupported float shapes rather than emitting
  integer code for them.

## [0.4.0] — 2026-11-19

### Added

- Live homogeneous float-table J4 roots (`TGETF`, `FMULK`, `FADD`), including
  float constants such as `t[i] * 2.0`.
- J4 CALL-exit prefixes: non-inlineable calls execute the natively supported
  prefix, resume the interpreter at the CALL, and continue the loop.
- J4 benchmarks for float tables and CALL exits.

### Changed

- J4 table traces keep tag guards, including collectable table tags.
- Integer J4 roots reject table-typed arithmetic operands and float constants
  that have no integer specialization; such traces fall back safely.
- J4 helper failures preserve their exact exit PC across C helper calls.

### Fixed

- `x = t[i] * 2.0` no longer loses the double constant (integer `imul 0`).
- J4 stats now distinguish CALL exits (`ir_call_exits`).

## [0.3.0] — 2026-11-19

### Added

- Luar-only `j4` benchmark corpus covering integer, table-read,
  array→hash side-trace, one- and two-argument leaf-call, and multi-table
  paths; `bench/compare.sh` reports stock Lua/LuaJIT as skipped for `j4`.
- Two-argument integer leaf-call inlining in J4 roots.
- J4 benchmark and correctness coverage for exact side-failure resume,
  closure-guard fallback, cycle classification, `__newindex` fallback and
  table-write metatable rejection.
- J4 table traces now keep guards (collectable table tags) and report a
  per-instruction resume PC; native exit PCs are bounds-checked in `lvm`.
- The shared-library export map is an explicit public `luar.h` allowlist;
  macOS builds use `.dylib` names and `@rpath` install names.
- `make dist` requires a clean tree; staged install CI compiles and runs
  shared and static consumers through the installed `luar.pc`.

### Changed

- J4 side linking now records the root start separately from the native
  backedge for correct cycle classification.
- Side-code cache exhaustion no longer leaks generated code.
- Benchmark documentation no longer describes Luar as interpreter-only.

## [0.2.0] — 2026-11-18

### Added

- The default build level is now `-O3` (overridable with `CFLAGS`); it brings
  the interpreter `mandel` path to stock-Lua parity without changing
  semantics.
- FFI cdata bitwise/shift operators (`& | ~ << >>`, `__band`..`__bnot`),
  `//` and `^` with LuaJIT-compatible 64-bit promotion and masked shifts.
- FFI parser hardening: constant-expression arithmetic wraps in 64 bits,
  `INT64_MIN / -1` and `% -1` no longer trap, and deeply nested declarators
  or constant expressions raise a Lua error instead of overflowing the C
  stack, and function type ids are sized from their parts (no buffer
  overflow for long type names).  Found by the new deterministic cdef
  fuzzer.
- `string.format` integration for cdata: integer conversions and
  `%s`/`%q` follow LuaJIT; plain Lua values are unchanged.
- `ffi.gc(cb, fn)` now runs `fn` for callback cdata before releasing the
  closure, and `string.format` skips its cdata rewrite when no argument is
  cdata.
- Unions of any size up to 16 bytes, including mixed SSE/INTEGER
  eightbytes, odd sizes (9–15) and array/struct members, and structs
  containing them can be passed and returned by value.  Larger unions use
  the memory ABI when their size is a multiple of their alignment;
  oversized ones remain rejected.
- FFI structs with fixed array members (including arrays of structs) can be
  passed and returned by value; VLA/empty arrays and bitfield structs are
  rejected.
- `ffi.cdef` preprocessing: object-like and named/variadic function-like
  macros (`...`/`__VA_ARGS__`), `#undef`, nested argument substitution, and
  `#ifdef`/`#ifndef`/`#if`/`#else`/`#endif`, with macros expanded outside
  strings and comments and persisted across cdef calls.  Function-like
  macros, non-pack `#pragma` and complex `#if` expressions fail clearly;
  `<...>` includes use local resolution.  Token pasting (`##`) and parameter stringification (`#`)
  work in function macros, and `#error` text is expanded; quoted
  `#include "path"` resolves relative to the including file (cwd fallback)
  with include-guard/macro persistence and depth/size limits.  `<...>`
  paths use the same local resolution; there is no system include search.  `#pragma pack(n/push/pop)` sets byte-granular layout.
- Fixed the cdef preprocessor dropping the last character of indented lines
  (leading whitespace was subtracted from the expansion length twice).  This
  affected multiline enums and any indented declaration followed by commas.
- `ffi.cdef` constant expressions accept `sizeof(type)`, `_Alignof`,
  `offsetof` and `__builtin_offsetof`, including arrays and use inside
  array bounds/enums.
- `ffi.istype` follows LuaJIT pointer compatibility: a struct/union accepts
  a pointer to it and same pointer types match; `void *` has no special case.
- `ffi.offsetof` reports LuaJIT's bitfield form: storage-unit byte offset,
  bit position within the unit and width; byte-packed structs report the
  containing byte and bit offset within it.
- Callback cdata supports LuaJIT's `cb:set(function)` and `cb:free()`
  methods; freeing invalidates the pointer and later use is a clear error.
- `__attribute__((packed))`/`aligned(n)` are accepted before the struct tag
  as well as after the body; byte-granular/over-aligned layout and field
  access work.  On x86-64, a packed integer struct with one by-value
  argument and scalar return is called through a MEMORY-ABI stack thunk;
  other packed aggregates are rejected by value rather than mislayed.
- FFI bitfields for integer/enum/`_Bool` struct fields: GCC/x86-64
  bit-granular packing including mixed base types, byte-crossing fields,
  unnamed padding and zero-width alignment fields, signed extraction, masking on set, positional/named
  initializers; struct and union bitfields share the same layout engine,
  and bitfield structs can be passed/returned by value through grouped
  synthetic integer storage fields.
- `__thread`/`_Thread_local` globals are supported through the existing
  dlsym symbol path, with read/write access from Lua.
- `ffi.callback` also returns small scalar unions by value when the Lua
  callback returns a matching cdata.
- `ffi.callback` returns structs by value when the Lua callback returns a
  matching cdata, and also handles struct-by-value arguments reaching Lua
  (covered by the helper's `call_pair_cb`).
- `ffi.callback`: libffi closures that call Lua functions, with GC
  lifetime, error capture through `ffi.callback_error()`, and clear
  rejection of aggregate-return/variadic callback types.
- A real-consumer FFI test: `tests/consumer/luatorch.lua` loads the
  libtorch-backed `libluatorch.so` C ABI and runs tensor creation, `lt_add`,
  scalar reads and data transfer through `ffi.load`.
- Deterministic JIT differential fuzzing (`tests/jit/04_fuzz.lua`): 150
  generated chunks compare interpreter and compiled results bit-for-bit,
  including value flavours and mixed int/float arithmetic.
- JIT differential/stress coverage (`tests/jit/03_guards.lua`): every
  compiled-loop case is compared against an interpreter run, including
  overflow, empty loops, division, negative zero, `break` and type-changing
  call sites.  `make JIT=0 test` verifies the compiled-out configuration.
- J2 `for` loops now support native `break` exits (the compiler's `CLOSE`
  is a no-op when the whitelisted body has no closures); conditional
  `while` backedges remain conservative.
- Build: `-MMD` header dependencies make `make` rebuild when `Proto`,
  JIT or emitter headers change.
- J2 while-loop differential fuzzing (`tests/jit/05_while_fuzz.lua`):
  100 generated stepped loops compare interpreter and native results.
- J2 `while` support: simple integer-conditioned `while` loops (body without
  calls/tables/float conditions) run their remaining iterations natively;
  `break` and float conditions fall back.
- Small, HFA, mixed and large by-value unions plus packed aggregate
  arguments/returns/callbacks and Darwin TLS are enabled and tested on
  arm64.
- The macOS CI job now builds and tests the full FFI suite; `ffi.load`
  resolves `lib*.dylib`, and x86-64-only union/packed/TLS tests are gated
  behind the architecture.
- J3 raw table fill/sum intrinsics (default, `tint=0` disables):
  `t[i]=i` and `acc=acc+t[i]` loops are executed by C helpers, making
  `table/array` ~0.14x stock Lua (and faster than LuaJIT).
- Guard-exit driven loop variants: when a compiled numeric loop fails its
  type guards, a second variant is compiled from the live frame if a slot
  is free and `hotexit` exits have accumulated; successful variants are
  counted by `jit.stats().side_traces` (`jit.opt.start("hotexit=N")`,
  default 10) (int/float polymorphic loops); both variants are tried at each
  backedge.  `tests/jit/25_polymorphic.lua` covers it.
- Inline a simple closure shape: `f` with one integer upvalue doing
  `x = x + arg`, called for its side effect in a numeric loop.  The native
  loop updates the open upvalue directly; `object/closures` drops to ~0.01x
  stock Lua and beats LuaJIT.  Other call shapes still interpret.
- Inline a simple `__index` multiplier shape: an empty table with an
  `__index` function returning `k * N` is compiled to `acc += idx * N`,
  re-validating the table/metatable/function body at every native entry.
  `object/metatable` drops to ~0.01x stock Lua and beats LuaJIT.
- Inline a one-field method shape: `obj:add(i)` where the method body is
  `self.v = self.v + x` is executed by a C loop that computes the final field
  value and stores it once.  Handles table registers and upvalues;
  `object/method` drops to ~0.02x stock Lua and beats LuaJIT.
- Documented the entry-guard placement limitation preventing prefixed table
  loops from producing non-root side exits.
- Live side tests now assert depth and root-cycle classification.
- Documented the entry-guard placement limitation preventing prefixed table
  loops from producing non-root side exits.
- Live side tests now assert depth tracking as well as compilation and
  execution.
- Added guarded inlining of one-argument integer leaf calls in IR roots.
- Declared the J4 mid-loop side-trace milestone complete in the J4 document.
- Added a deterministic IR/side differential test over array/hash shapes.
- Added `__index` fallback coverage for table IR paths.
- Added mixed table/call interior side-trace coverage.
- Added multi-table interior side exits and recorded side depth.
- Added periodic interpreter yields (safepoints) for generated root traces,
  counted by `ir_yields`.
- Fixed `MMBINI` immediate-slot bounds checking, enabling prefixed table
  bodies in IR recording and true interior side exits (asserted by
  `tests/jit/45_ir_interior.lua`).
- Linked live side traces from IR-root table guard exits, including
  snapshot/restore and counters.
- Added live array-to-hash side traces: a guard exit compiles a generic
  hash-`TGET` side segment, executes it with snapshot/restore, and continues
  the loop.  Added `tests/jit/43_side_live.lua`.
- Added a generic hash-lookup primitive for future array-miss side traces.
- Expanded exit snapshots to 64 slots and added a restore selftest.
- Added side-failure/restore coverage for interior segments.
- Added the side-continuation emitter contract: interior segment + FORLOOP
  update returning continue/done/failure, with a selftest.
- Added live table-read and table-write IR execution (`TGET`/`TSET`) with
  table/key validation; array growth and float arithmetic fall back.
- Added IR fallback coverage for table loops and reset `ir_executed` on
  `jit.flush()`.
- Added opt-in live execution of generated root traces (`ir=1`) and fixed the
  DynASM arithmetic action scoping that had dropped the ADD store/tag.
- Added a trace-entry guard contract (`luar_trace_ir_guards_match`) and
  selftest for live-entry validation.
- Added interior-segment IR execution coverage via
  `jit.trace_ir().side_exec_selftest()`.
- Added integer trace-IR execution coverage via
  `jit.trace_ir().exec_selftest()`.
- Added integer trace-IR to x86-64 machine-code emission, compiled as a
  validation shadow (`jit.stats().ir_compiled/ir_code_bytes`).
- Added J4 side-link metadata: depth cap 4, root-cycle detection and IR
  type/liveness analysis.
- Added J4 interior guard-exit IR recording (one segment per Proto) and
  table/K-op IR ops, counted by `jit.stats().ir_side_traces/ir_side_ops`.
- Added a J4 numeric-loop IR recorder wired into the hot hook, counted by
  `jit.stats().ir_traces`, `ir_ops` and `ir_aborts`.
- Added the independent J4 tracing-IR foundation (`luar_trace.c`): typed
  values, guards, exits, a verifier, live-slot exit snapshots/restore and a
  selftest exposed as `jit.trace_ir()`.
- Code slots record their entry guard tags and dispatch to the matching
  variant (`jit.stats().sig_hits`), avoiding calls into a known-mismatched
  variant.
- Mid-loop table guards now return the exact failing bytecode PC (reported as
  `jit.stats().last_exit_pc`) and resume the interpreter at that instruction,
  preserving prior iteration state.  This is side-exit state capture, not a
  recorded side trace.
- Inline branchy two-parameter leaves through the generic integer evaluator.
- Inline four-parameter leaf calls through the bounded evaluator.
- Inline branchy three-parameter leaves through the generic evaluator.
- Inline three-parameter leaf calls (`a*b+c`, `a*b*c`, `a+b+c`, `a-b-c`)
  whose result feeds a numeric accumulator; second and third arguments are
  read from their loop-body slots and guarded as integers.
- Inline simple branch-containing integer leaves: comparison + JMP arms
  (`if x > K then return x*2 else return x+1`), plus a bounded evaluator for
  short branchy bodies with comparisons, JMP, booleans and arithmetic.
- Inline two-parameter straight-line integer leaves feeding a numeric
  accumulator, including `a*b+b`, `a*b`, `a+b` and `a-b`; the second argument
  is read from its loop-body slot and guarded as an integer.
- Inline one-parameter leaf arithmetic calls feeding a numeric accumulator
  (`s = s + f(i)`), including straight-line integer chains with affine
  `x*K+C` fast folding and a bounded general evaluator for
  `x*K`, `x+K`, `x*x`, `x+x`, `(x+K)*K`, squares plus offsets and similar
  leaf bodies.  The closure Proto
  and bytecode are revalidated each entry; `object/leaf_call` runs at ~0.04x
  stock Lua and beats LuaJIT.
- Keyed raw table fill/sum intrinsics (`t[keys[i]] = i`,
  `s = s + t[keys[i]]`) with hash presizing: `table/hash` drops to ~0.43x
  stock Lua.  Keys may be function parameters or closure upvalues.
- The hot-loop hook now recognises a Proto whose first loop is an intrinsic,
  so subsequent loops in the same function can be recorded and intrinsic
  compiled too.  Generic table emission remains opt-in
  (`jit.opt.start("table=1")`): raw integer array
  stores (`t[i] = i`) and accumulation (`acc = acc + t[i]`), including
  register-cached fill/sum fast paths,, with metatable/bounds/tag guards
  that bail before the current iteration.  Both are correct but not yet
  faster than the interpreter, so they are off by default.
- Early J3 coverage: a read-only integer raw-array lookup
  (`t[i]`, or fused `acc = acc + t[i]`) compiles for one table at the body
  start, with bounds/tag guards that fall back before the current iteration.
  It is experimental and not yet faster than the interpreter.
- J4 peephole: `for` loops with `acc = acc * i` keep the accumulator and
  loop counters in GPRs (integer or float accumulator).
- J4 peephole: `for` loops with `acc = acc + i * i` keep acc/index/count
  in GPRs (integer or float accumulator); a direct 20M-iteration
  sum-of-squares runs ~16x faster than the interpreter.
- J4 peephole: `while i < limit do acc = acc + i; i = i + 1 end` keeps the
  accumulator/index/limit in GPRs; prefix-sum while loops run ~12x faster
  than the interpreter in a direct 20M-iteration measurement; the
  increment can be any nonzero immediate (e.g. `i = i + 2`), and a float
  accumulator keeps `acc = acc + i * const` in an XMM register.  The same
  fast path recognises `acc = acc + i * i` (~10x), including float
  accumulators, and product loops
  `acc = acc * i` with integer or float accumulators.
- J4 peephole: `acc = acc + i` integer loops keep the accumulator and loop
  counters in GPRs; `numeric/int_sum` drops from ~5.9 ms to ~3.9 ms (0.11x
  stock Lua, faster than LuaJIT).
- J4 peephole: `acc = acc + i * const` now covers integer and float
  accumulators; integer constants beyond 32 bits are loaded once.
- J4 peephole: `acc = acc + i * const` float loops keep the accumulator in
  an XMM register and the loop counters in GPRs; `numeric/float_loop` drops
  from ~11 ms to ~6.8 ms (0.25x stock Lua).
- J2 float/double comparisons (`<`, `<=`, `>`, `>=`, `==`, `!=`, including
  NaN-false semantics) work in native `if` branches and `while` conditions.
- J1/J2 can keep two native loops per `Proto` (first and most recent),
  with a bloom of attempted backedges; functions with sequential numeric
  loops compile more than one.
- `jit.opt.start("hotloop=N")` configures the hot threshold for newly
  created Protos.
- `jit.dump("t"/"i")` prints recorded bytecode snapshots and linear IR to
  stderr, matching the LuaJIT-style debugging surface without file/function
  sinks.
- J2 branch support: integer comparisons (`<`, `<=`, `>`, `>=`, `==`, `!=`)
  and forward `if`/`else` jumps inside a whitelisted loop body are emitted as
  guarded native branches; float comparisons and backward inner loops fall
  back to the interpreter.
- J2 native loops (x86-64): a hot `for` or `while` loop whose body is numeric register
  arithmetic (MOVE/LOADI/LOADF/LOADK/ADDI/ADD/SUB/MUL/DIV, the K variants
  and MMBIN, with int/double conversions) is compiled with DynASM and runs
  its remaining iterations directly on the Lua stack slots, with tag guards
  and exact FORLOOP semantics.  In-place accumulator peepholes bring
  `numeric/int_sum` to LuaJIT parity (5.9 ms) and `float_loop` to 11.1 ms;
  integer constants used in float arithmetic convert correctly, and
  type-changing loop-carried registers fall back to the interpreter.  `jit.status()` is now `true` and
  `luar.features.jit` is enabled on x86-64; other hosts keep the interpreter.
- J1 trace recorder: hot-loop counters, trace selection, bytecode
  snapshots, `jit.stats()`, `jit.trace()`, `jit.traces()`, a linear IR
  dump (`jit.ir()`), working `jit.flush()`/`jit.off()` recording control.
  No machine code is generated and `jit.status()` remains `false`.
- `make shared` builds `libluar.so.0.1.0` with SONAME `libluar.so.0` and the
  `libluar.so.0`/`libluar.so` symlinks, from separate `.pic.o` objects. An
  ELF export map (`etc/libluar.map`) limits the dynamic symbol table to the
  Lua 5.5 C API and `luar_*`. `make install` installs it next to
  `libluar.a`; `make test-shared` links an embedding host and the CLI
  against it and loads a stock Lua 5.5 C module. `make clean` removes the
  shared library, symlinks and PIC objects.

## [0.1.0] — 2026-11-17

The first Luar release: Lua 5.5.1 only, a LuaJIT-style surface on top, and
the machinery to keep following Lua.

### Added

- `luar` and `luarc` binaries, `libluar.a`, installable with `PREFIX`,
  `luar.pc` and a man page. Bytecode is the official Lua 5.5.1 format and is
  interchangeable with `lua`/`luac` 5.5.1.
- The Lua 5.5.1 core, vendored pristine in `vendor/lua-5.5.1/` and derived
  into `src/`, with the full language: integers, `<const>`/`<close>`,
  `global` declarations, read-only loop variables, `table.create`, and the
  5.5 standard library.
- `luar` identity table (`version`, `lua_version`, `features`, `os`,
  `arch`), `LUAR_INIT`, and `luar/5.5` module directories ahead of the Lua
  ones.
- `table.new` and `table.clear`.
- FFI (0.2 preview, `WITH_FFI=1`): `cdef` parser, C type model and layout,
  cdata allocation/access, int64/uint64, metatypes and finalizers,
  `ffi.C`/`ffi.load` calls through libffi (struct-by-value and variadic
  calls included), `ffi.errno`, plus `type()`/`tonumber()` integration and
  NULL-pointer semantics.
- `jit` control surface: `version`, `version_num`, `os`, `arch`, `status`,
  `on`, `off`, `flush`, `opt.start`. `jit.status()` truthfully returns
  `false`: no native code generator ships in 0.1.
- Extended `luaL_openlibs` that opens the Luar extensions for embedders.
- LuaJIT-compatible CLI: `-j on|off|flush|opt=...|v` and
  `-b [-s] input output` bytecode compilation.
- Stock Lua 5.5 C modules load unchanged (C ABI preserved, exported symbols);
  `tests/cli.sh` builds one with the system headers and runs it.

### Documentation

- `doc/architecture.md` — value model, tiers, why the Lua 5.5 core is the
  semantic authority, upstream-follow design.
- `doc/roadmap.md` — 0.1 → 1.0 milestones with exit criteria.
- `doc/lua55-only.md` — the practical 5.5-only guide: reserved words,
  native bitwise operators, removed legacy names, strict C API.
- `doc/compatibility.md` — the 5.5-only contract and the LuaJIT API matrix.
- `doc/testing.md` — suite layout, differential testing, sanitizers.
- `doc/upstream.md` — vendor/baseline policy and the Lua 5.6 adoption
  checklist.
- `doc/adr/0001`–`0003` — core choice, libffi, upstream deltas.

### Tests and tooling

- Semantics suite that also passes under stock Lua 5.5.1, including a
  native-bitwise-operator suite and a strict-5.5 regression suite.
- Differential suite: identical stdout and exit status under `lua` and
  `luar`.
- Official Lua 5.5.1 test suite runner (`tests/upstream/run.sh`); basic mode
  passes 31/31 files and the canonical `final OK !!!`.
- CLI/ABI/embedding tests, and `tools/upstream-check.sh` +
  `tools/upstream-sync.sh` with a checked-in delta baseline.
- GitHub Actions CI for Linux (and an optional macOS job).

### Removed

- The vendored LuaJIT 2.1 tree (`vendor/luajit/`).  Only DynASM was kept, in
  `dynasm/`, and its backends were ported from the Lua 5.1 `bit` library to
  native Lua 5.5 operators (byte-identical generated code).
- The LuaJIT-compatible `bit` library.  Lua 5.5 has native `&`, `|`, `~`,
  `<<` and `>>`; a 5.1-era shim does not belong in a 5.5-only runtime.
- Every `LUA_COMPAT_*` switch and code path: `LUA_COMPAT_GLOBAL`,
  `LUA_COMPAT_MATHLIB`, `LUA_COMPAT_APIINTCASTS`, the legacy API macros
  (`lua_strlen`, `lua_objlen`, `lua_equal`, `lua_lessthan`,
  `luaL_checkint`, ...), and the deprecated math functions.
- `doc/porting-from-luajit.md`; Luar documents only the current Lua line.

### Known limitations

- The FFI is opt-in and still 0.2-preview: bitfields, preprocessor macros
  and C→Lua callbacks are not implemented (see `doc/ffi.md`).
- No native JIT. Performance is stock Lua 5.5 interpreter performance; see
  `doc/performance.md` for measured numbers against Lua 5.5.1 and LuaJIT 2.1.
- Only Lua 5.5 is supported.  There is no compatibility mode for any older
  Lua version, and LuaJIT's C API/bytecode are not supported.
