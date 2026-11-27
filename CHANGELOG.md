# Changelog

All notable changes to Luar are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow semver for
the runtime, while the Lua language surface follows upstream Lua.

## Unreleased

### Added

- J2 updates 8-byte integer struct fields in place with small integer
  constants: `s.f = s.f +|-|*|&|||~|<<|>> k`.  The sibling fields are
  preserved and the visible element temporary is a cdata scalar.
- The same field updates support signed/unsigned `//` and `%` constants;
  zero divisors use the cdata `INT64_MIN` result.
- 8-byte struct fields also update with a cdata scalar register/upvalue:
  `s.f = s.f +|-|*|&|||~|<<|>> V`.
- Struct fields held in a closure-upvalue cdata object update in place:
  `U.f = U.f +|-|*|&|||~|<<|>> k`, compiled through GETTABUP/SETTABUP.
- Those upvalue-field updates also accept a cdata scalar register/upvalue
  (`U.f = U.f OP V`).
- J2 reduces 8-byte struct fields into a cdata accumulator:
  `acc = acc +|-|*|&|||~|<<|>> s.f`, covering register struct fields and
  closure-upvalue struct fields (`GETTABUP`).
- Field reductions also support signed/unsigned `//` and `%`; zero fields
  use the cdata `INT64_MIN` result.
- J2 copies 8-byte struct fields directly (`dst.f = src.f`), including
  same-struct, cross-struct, uint64 and self-copy forms.
- Field copies accept a closure-upvalue source and/or destination
  (`U.f = dst.f`, `dst.f = U.f`, `U.f = V.f`).
- J2 fills 8-byte struct fields with integer constants (`s.f = k`),
  including register/upvalue structs, uint64 fields and negative values.
- Narrow integer fields also fill from constants: int32_t, int16_t and
  int8_t fields, with sibling fields preserved.
- Narrow integer fields fill from cdata scalar registers/upvalues of
  1/2/4/8-byte integer types.
- Narrow integer fields update in place with constants: add/subtract/
  multiply/bitwise/shift on int32/int16/int8 fields, with width truncation
  and sibling fields preserved.
- Narrow fields also update from cdata integer scalar registers/upvalues,
  including bitwise and shift operations.
- Narrow integer fields copy directly (`s.a = t.b`) for matching 1/2/4-byte
  field widths, including self-copy and sibling preservation.
- Cross-width narrow copies sign-extend signed sources and truncate to the
  destination width (int16/int8/int32 combinations).
- Narrow field copies work through a closure-upvalue struct source or
  destination, including both-upvalue and cross-width forms.
- Narrow fields update in place through an upvalue struct destination:
  int32/int16/int8 add/subtract/multiply/bitwise/shift with width
  truncation.
- Narrow signed/unsigned fields support constant `//` and `%` updates with
  width sign-extension/truncation.
- Narrow upvalue struct fields also update from cdata integer scalar
  registers/upvalues.
- Narrow integer fields fill from Lua integer scalars, including negative
  values and int32/int16/int8 destinations.
- Narrow fields also fill from Lua floating scalars with C truncation
  toward zero, falling back on non-finite/out-of-range values.
- `_Bool` struct fields fill from numeric scalars with C truth
  normalization (any non-zero value becomes true).
- `_Bool` fields also fill directly from Lua boolean registers/upvalues.
- `_Bool` fields copy directly, including upvalue source/destination
  forms; the visible temporary is a Lua boolean.
- Field fills accept cdata `_Bool` scalars, storing 0/1 in bool and
  integer destinations.
- `_Bool` fields fill from constant `true`/`false` K values.
- Enum fields fill from integer constants, numeric/cdata scalars and
  upvalue struct destinations, using the 4-byte unsigned enum view.
- Enum fields copy to/from enum and integer fields, including upvalue
  sources; the visible temporary is a Lua integer.
- Double struct fields fill from numeric constants (`s.x = 2.5`),
  including register/upvalue destinations and integer promotion.
- Double fields also fill from Lua-number and cdata double scalar
  registers/upvalues.
- Double fields update in place with float constants:
  `s.x = s.x +|-|*|/|% k`, preserving sibling fields.
- Double fields also update from Lua-number and cdata double scalar
  registers with the same operations.
- Double struct fields copy directly (`dst.x = src.y`), including
  same-struct, self and cross-struct forms.
- Double field copies accept closure-upvalue sources and destinations,
  including both-upvalue and self-copy forms.
- Float fields fill from numeric constants (`s.x = 2.5`), including
  register/upvalue destinations and integer promotion.
- Float fields also fill from Lua-number and cdata float scalar
  registers/upvalues.
- Float fields update in place with float constants:
  `s.x = s.x +|-|*|/|% k`, preserving sibling fields.
- Float fields also update from Lua-number and cdata float scalar
  registers.
- Float struct fields copy directly (`dst.x = src.y`), including
  same-struct, self and cross-struct forms.
- Float field copies accept closure-upvalue sources and destinations,
  including both-upvalue and self-copy forms.
- Mixed float/double field copies convert between 4-byte and 8-byte
  fields (`dst.d = src.f` and `dst.f = src.d`).
- Mixed integer/float field copies convert with C truncation
  (`dst.d = src.i`, `dst.i = src.d`, etc.) for int fields up to 4 bytes.
- Float/double fields fill from cdata integer scalars, including signed
  negatives and unsigned values.
- Float/double fields update in place from cdata integer scalars with
  add/subtract, including upvalue destinations.
- Upvalue float fields update from Lua-number/cdata float scalar
  registers with add/subtract/multiply/divide/modulo.
- Upvalue float/double fields update from cdata integer scalars with
  add/subtract.
- Upvalue float fields also update with float constants
  (`U.x = U.x OP k`).
- Double field scalar updates work through an upvalue struct destination
  (`U.x = U.x OP v`).
- Upvalue double fields also update with float constants
  (`U.x = U.x OP k`).
- Narrow fields update from Lua integer scalars: int32/int16/int8
  add/subtract/multiply/bitwise/shift with width truncation.
- Narrow fields fill from cdata float/double scalars with C truncation
  toward zero, falling back on non-finite/out-of-range values.
- Field fills also accept a cdata scalar register/upvalue (`s.f = V`),
  including uint64 fields and negative values.
- J2 updates bounded double arrays in place with float constants:
  `a[i] = a[i] +|-|*|/|% k`, returning the last element as a cdata temp.
- The same constant updates work on bounded float32 arrays, rounding
  through the 4-byte element.
- Float/double array updates accept Lua-number scalar registers/upvalues
  with add/subtract/multiply/divide/modulo, including float32 elements.
- J2 fills bounded double arrays from a cdata double scalar register or
  upvalue (`a[i] = V`), including NaN and infinity values.
- Numeric constant fills (`a[i] = k`) are also covered for double arrays,
  including negative, integer, zero and large constants.
- Double-array fills accept Lua-number register/upvalue scalars as well as
  cdata doubles.
- J2 fills bounded float32 arrays (`a[i] = k/V`) with constants,
  Lua-number scalars and cdata float scalars, including negative and zero
  values.
- Float/double array fills work through a closure-upvalue array
  destination, with register/upvalue scalars and float32/double elements.
- Upvalue float arrays also fill from numeric constants
  (`U[i] = 2.5`), including negative, integer, zero and float32 values.
- J2 copies float/double arrays into an upvalue destination (`U[i] = s[i]`),
  including float/double conversion, strided and self-copy forms.
- Float/double arrays also copy from an upvalue source (`d[i] = U[i]`),
  including both-upvalue copies and float/double conversion.

- FFI accepts field-level `__attribute__((aligned(n)))` and
  `__attribute__((packed))`; the layout uses the field alignment override
  for offsets, struct size and alignment.  `aligned` takes precedence over
  `packed` in either order, and aligned bitfields start on the requested
  boundary and size the enclosing struct accordingly.

- J2 in-place 8-byte cdata updates accept IDIV/MOD with an upvalue cdata
  scalar on register arrays: signed/unsigned `a[i] = a[i] // V` and
  `% V`.  Zero scalars use the cdata `INT64_MIN` bit pattern.

- J2 in-place 8-byte cdata updates through an upvalue-array destination
  accept a cdata scalar register/upvalue for `//` and `%`.  Signed
  division truncates toward zero; zero scalars use the cdata `INT64_MIN`
  bit pattern.

### Tests

`tests/ffi/196_j2_i64_update.lua` extended with signed/unsigned
register/upvalue-array divide/modulo and zero-scalar cases.

## [0.241.0] — 2026-11-24

### Added

- J2 in-place 8-byte cdata updates through an upvalue-array destination
  accept a cdata scalar register/upvalue:
  `U[i] = U[i] +|-|*|&|||~|<<|>> V`.  Runtime helpers validate the scalar
  and array types without dereferencing record-time registers.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/ffi/196_j2_i64_update.lua`.

## [0.240.0] — 2026-11-24

### Added

- J2 in-place 8-byte cdata constant updates work through a closure-upvalue
  array destination: `U[i] = U[i] OP k` for add/subtract/multiply/bitwise
  and masked shifts with int32 K/immediate constants.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/ffi/196_j2_i64_update.lua`.

## [0.239.0] — 2026-11-24

### Added

- J2 in-place 8-byte cdata updates accept a closure-upvalue cdata scalar:
  `a[i] = a[i] +|-|*|&|||~|<<|>> V`.  The scalar is re-resolved each entry;
  int64/uint64 destinations and shifted counts are covered.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/ffi/196_j2_i64_update.lua`.

## [0.238.0] — 2026-11-24

### Added

- J2 in-place 8-byte cdata updates accept IDIVK/MODK integer constants:
  signed and unsigned `a[i] = a[i] // k` and `% k`.  Zero constants use
  the interpreter cdata `INT64_MIN` bit pattern; constants remain int32.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/ffi/196_j2_i64_update.lua`.

## [0.237.0] — 2026-11-24

### Added

- J2 in-place 8-byte cdata updates accept small integer immediate/K
  constants: `a[i] = a[i] +|-|*|&|||~|<<|>> k`.  K scalars are restricted
  to int32; shift counts use the cdata mask-to-six-bits rule.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/ffi/196_j2_i64_update.lua`; the J4 shift baseline now disables
tint, and the older cdata-store test expects the new helper.

## [0.236.0] — 2026-11-24

### Added

- J2 in-place 8-byte cdata updates support signed/unsigned `//` and `%`
  with a cdata scalar.  Signed division truncates toward zero,
  `INT64_MIN / -1` wraps, and a zero divisor uses the interpreter's cdata
  result (`INT64_MIN` bit pattern).

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/ffi/196_j2_i64_update.lua`.

## [0.235.0] — 2026-11-24

### Added

- J2 updates bounded 8-byte integer cdata arrays in place with a cdata
  scalar: `a[i] = a[i] +|-|*|&|||~|<<|>> k`.  int64/uint64 destinations,
  shifted counts masked to six bits, and cdata element temporaries are
  supported.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/196_j2_i64_update.lua`.

## [0.234.0] — 2026-11-24

### Added

- J2 reduces bounded 8-byte integer cdata arrays with signed/unsigned
  `//` and `%` into a cdata accumulator.  Signed division truncates toward
  zero, `INT64_MIN / -1` wraps, and a zero divisor is pre-scanned so the
  run falls back before changing the accumulator.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/195_j2_i64_divmod_sum.lua`.

## [0.233.0] — 2026-11-24

### Added

- J2 reduces bounded 8-byte integer cdata arrays with bitwise AND/OR/XOR
  and logical left/right shifts into a cdata accumulator.  Shift counts
  use the cdata mask-to-six-bits rule; int64/uint64 accumulators,
  strided/descending runs and upvalue arrays are supported.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/194_j2_i64_bitwise_sum.lua`.

## [0.232.0] — 2026-11-24

### Added

- J2 multiplies bounded 8-byte integer cdata arrays into a cdata
  accumulator with wrap-defined raw 64-bit arithmetic.  int64/uint64
  accumulators, strided/descending runs and upvalue arrays are supported.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/ffi/193_j2_i64_sum.lua`.

## [0.231.0] — 2026-11-24

### Added

- J2 reduces bounded `int64_t`/`uint64_t` cdata arrays into a cdata
  accumulator with raw add/subtract.  Register/upvalue arrays, strided and
  descending runs are supported; the accumulator and element temporary
  keep their cdata types.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/193_j2_i64_sum.lua`.

## [0.230.0] — 2026-11-24

### Added

- J2 fills 8-byte integer cdata arrays from an int64/uint64 cdata scalar
  value.  Register and closure-upvalue values/destinations are supported,
  along with strided and descending runs; the value is copied raw.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/192_j2_i64_fill.lua`.

## [0.229.0] — 2026-11-24

### Fixed

- J2 raw 64-bit cdata copies now keep the visible element temporary as a
  scalar cdata with the value inline; the previous owned-payload object
  exposed pointer bits instead of the last element.
- The temporary slot is re-resolved from the current frame base after the
  cdata allocation, so a stack reallocation cannot leave a stale pointer.
- Edge coverage for int64→uint64 destinations, self-copy and
  out-of-bounds fallback.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/ffi/191_j2_i64_copy.lua`.

## [0.228.0] — 2026-11-24

### Added

- J2 raw 64-bit cdata element copies: `dst[i] = src[i]` for
  `int64_t`/`uint64_t` arrays copies 8 bytes directly and exposes a cdata
  element temporary.  Register/upvalue sources and destinations, mixed
  signedness and strided/descending runs are supported.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/191_j2_i64_copy.lua`; `tests/ffi/137_j2_cdata_i64_fill.lua`
now expects the raw copy helper.

## [0.227.0] — 2026-11-24

### Added

- Affine/index table fills support any nonzero integer step, including
  strided and descending loops, for register and closure-upvalue
  destinations.
- Affine fills accept register and upvalue scalars for add/subtract/
  multiply, not only small Proto constants.
- A float scalar promotes the filled values to floats (`i + 2.5`,
  `i * 2.5`, `i - 2.5`) while preserving the interpreter's arithmetic.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  Extended coverage:
`tests/jit/221_table_affine_fill.lua`.

## [0.226.0] — 2026-11-24

### Added

- J2 recognises table affine/index fills directly: `t[i] = i`, `i + k`,
  `i - k` and `i * k`.  The direct matcher runs even when the general
  decoder has not selected a fill, including a closure-upvalue destination
  (`U[i] = i` / `i +/- k` / `i * k`).

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/jit/221_table_affine_fill.lua` and the upvalue identity case in
`tests/jit/220_table_generic_keys.lua`.

## [0.225.0] — 2026-11-24

### Fixed

- Sparse nil-range clears pass the Lua key (not the zero-based loop
  offset) to `luaH_setint`; clearing `t[1000..1200]` no longer misses the
  range start/end.
- Generic table copies, fills and nil clears accept negative/hash integer
  keys, with a separate endpoint helper and a `1 << 22` iteration cap.
- Numeric constant fills through a closure-upvalue destination
  (`U[i] = 7`) use the generic fill path instead of staying interpreted.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/jit/220_table_generic_keys.lua`.

## [0.224.0] — 2026-11-24

### Added

- J2 table element copies handle arbitrary values: strings, tables,
  booleans and nil holes, for register and closure-upvalue destinations
  and sources.  Numeric copies keep the fast pair path; non-numeric or
  sparse runs use `luaH_getint`/`luaH_setint`.
- J2 table fills accept non-numeric constant, register and upvalue values
  (`t[i] = "x"`, `t[i] = true`, `t[i] = obj`, `t[i] = U`), including
  closure-upvalue destinations.  Numeric fills keep the affine helper.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/jit/218_table_value_copy.lua` and
`tests/jit/219_table_value_fill.lua`.

## [0.223.0] — 2026-11-24

### Added

- J2 table arithmetic stores support a closure-upvalue destination:
  `U[i] = src[i] OP k`, `U[i] = a[i] OP b[i]` and
  `U[i] = -src[i]`/`~src[i]`.  The destination slot is refreshed by the
  first interpreted iteration and re-checked on every entry.
- J2 bounded-cdata stores support a closure-upvalue destination for
  copies (`U[i] = src[i]`), scalar/bitwise stores, two-source stores and
  unary `-`/`~` forms.  Rebinding the upvalue or a non-cdata destination
  falls back cleanly.

### Fixed

- J4 `CPSETI`/`CPSETF`/`CPSETI64` pointer-field element writes dispatch to
  the matching `cset_ptr_*` helper.  They previously fell through the
  `CCALL0D` case and called `ccall0_double`, so the fused write always
  failed and exited to the interpreter.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/jit/217_table_upval_arith_store.lua`,
`tests/ffi/189_cdata_upval_dest_store.lua` and
`tests/ffi/190_j4_ptr_field_store.lua`.

## [0.222.1] — 2026-11-24

### Fixed

- J4 re-checks the integer FORLOOP control triplet at IR entry.  A
  float-controlled loop reaching the same pc/base no longer reuses an
  integer trace, which could return a wrong trip count or hang on the
  float payloads.
- A J4 `SLOAD` invalidates the destination's recorded type.  A following
  `ADD`/K-op now guards the live value and honors `__add`/`__mul` instead
  of doing raw integer arithmetic on a table's address bits.
- The same invalidation restores the cdata identity `CGUARD` before field
  reads/writes, so a later struct type with the same USERDATA tag cannot
  reuse an earlier field offset and corrupt an adjacent member.

New tests: `tests/jit/215_j4_forloop_float_reuse.lua`,
`tests/jit/216_j4_sload_stale_tag.lua` and
`tests/ffi/188_j4_cdata_cguard_sload.lua`.

## [0.222.0] — 2026-11-24

### Added

- FFI accepts fixed zero-length arrays (`T[0]`) as zero-sized types and
  struct members; `ffi.sizeof`/`ffi.new` report 0 and indexing is
  bounds-checked.
- FFI accepts a C99 flexible array member `T[]` as the final struct member,
  modeled as a zero-length array so the struct size excludes the payload;
  standalone `T[]` remains an incomplete variable-length type.
- J2 nil-range clears handle sparse tables: array slots are cleared
  directly and hash entries outside the array part are removed through
  `luaH_setint`, preserving unrelated keys.
- J2 table copies and nil clears support a closure-upvalue destination
  (`U[i] = src[i]`, `U[i] = nil`), re-resolved from the closure each entry.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/186_zero_length_arrays.lua`,
`tests/ffi/187_flexible_array_members.lua`,
`tests/jit/213_table_nil_sparse.lua` and
`tests/jit/214_table_upval_store.lua`.

## [0.221.0] — 2026-11-24

### Added

- J2 cdata scalar stores accept a closure-upvalue scalar
  (`a[i] = b[i] OP upvalue`), re-resolved on every entry, so rebinding it
  or changing an integer value is observed.
- J2 cdata stores accept a closure-upvalue source array for scalar,
  two-source, unary and copy forms (`a[i] = U[i]`, `a[i] = U[i] + V[i]`,
  `a[i] = -U[i]`, `a[i] = U[i] OP k`).  Mixed register/upvalue sources are
  supported.
- Scalar and copy stores distinguish register, immediate, constant and
  upvalue encodings; a GETTABLE destination may alias the upvalue-load
  temporary because it is reloaded next iteration.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/184_j2_cdata_upval_scalar.lua` and
`tests/ffi/185_j2_cdata_upval_source.lua`.

## [0.220.0] — 2026-11-24

### Added

- J2 runs two-source cdata bitwise stores
  `a[i] = b[i] &|~|<<|>> c[i]` over bounded integer arrays, including
  in-place destinations.  Negative shift counts reverse and 64 or more
  yields zero.
- J2 runs scaled cdata bitwise stores `a[i] = b[i] &|~|<<|>> k` with
  constant, immediate and register scalars.
- J2 runs cdata unary stores `a[i] = -b[i]` and `a[i] = ~b[i]`, with
  separate element/result temporaries and in-place, descending and strided
  ranges.

### Fixed

- J4 integer-key table reads initialize their result before
  `luaH_getint`; a miss now exits to `__index` instead of reusing the
  previous lookup's element when the same trace reads a missing key.
- J4 string-key reads and the float/table/bool/string field helpers do the
  same before `luaH_getstr`, fixing the corresponding stale-value miss.
- J4 always guards a dynamic `GETTABLE` key as an integer at the read
  point.  A record-time check was bypassable when an earlier boolean or
  string producer had already marked the key slot seen; the side helper
  could reinterpret the payload as an integer (`t[1.5]` or a boolean key
  reading `t[1]`).
- J4 no longer folds a mutable integer upvalue into a `CCALL2FF` argument.
  The record-time value froze `fmodf(k, 2.0)` calls after `k` changed; the
  live upvalue is read and converted instead.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/180_j2_cdata_bitwise_pair.lua`,
`tests/ffi/181_j2_cdata_bitwise_scalar.lua`,
`tests/ffi/182_j2_cdata_unary_store.lua`,
`tests/jit/209_j4_tgeti_miss.lua`,
`tests/jit/210_j4_tgetk_miss.lua`,
`tests/jit/211_j4_tget_key_type.lua`,
`tests/jit/212_j4_tget_key_guard.lua` and
`tests/ffi/183_j4_ccall2ff_upvalue.lua`.

## [0.219.0] — 2026-11-24

### Added

- J2 runs raw-table unary element stores `dst[i] = -src[i]` and
  `dst[i] = ~src[i]`; bitwise not requires integer elements.
- J2 runs two-source raw-table stores `dst[i] = a[i] OP b[i]` for
  add/subtract/multiply/divide/floor-divide/modulo, including in-place
  destinations and separate element/result registers.
- J2 runs raw-table bitwise stores: `dst[i] = src[i] &|~|<<|>> k` and
  `dst[i] = a[i] OP b[i]`, with constant, register, immediate and upvalue
  scalars.  Negative shift counts reverse; 64 or more yields zero; float
  operands stay interpreted.
- The runtime `ffi.offsetof` accepts dotted and indexed member designators
  (`i.b`, `a[1].x`) with the same bitfield result form as direct fields.

### Fixed

- J4 no longer passes a string/float/bool `SETTABLE` key to its
  integer-key TSET helper; such keys fall back instead of inserting a
  bogus integer key when a metatable is present.
- J4 constant integer `SETFIELD` values keep all 64 bits; the TSETKK IR
  path no longer rounds values above 2^53 through a double.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/179_runtime_offsetof_path.lua`,
`tests/jit/203_table_unary_store.lua`,
`tests/jit/204_table_pair_store.lua`,
`tests/jit/205_table_bitwise_store.lua`,
`tests/jit/206_table_pair_bitwise.lua`,
`tests/jit/207_j4_string_key.lua` and
`tests/jit/208_j4_tsetkk_i64.lua`.

## [0.218.0] — 2026-11-24

### Added

- J2 copies raw table array elements with `dst[i] = src[i]`, including
  in-place copies, destination growth and upvalue sources.
- J2 runs raw-table floor-division and modulo stores
  `dst[i] = src[i] // k` / `% k` with constant, register and upvalue
  scalars.  An integer zero divisor declines before any store so the
  interpreter raises the exact error; a float source follows the
  non-raising float path.
- J2 runs two-array floor-division and modulo reductions
  `s = s +|- a[i] // b[i]` / `% b[i]` over raw tables and bounded cdata
  arrays, with Lua's floor rules, integer wrap and float promotion.
- J2 clears an existing raw-table array range with `t[i] = nil` in one
  pass.  Partial, descending and strided ranges are supported; hash keys
  and entries outside the run are untouched, and metatables or runs past
  the array part fall back.
- `ffi.cdef` joins adjacent string literals in `sizeof`, so
  `sizeof("a" "bc")` is valid in static assertions, enums and array
  bounds.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and
`tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/177_sizeof_adjacent.lua`,
`tests/ffi/178_j2_cdata_pair_idiv.lua`,
`tests/jit/199_table_copy_store.lua`,
`tests/jit/200_table_idiv_store.lua`,
`tests/jit/201_pair_idiv.lua` and
`tests/jit/202_table_nil_store.lua`.

## [0.217.0] — 2026-11-24

### Added

- `ffi.cdef` constant expressions accept dotted and indexed `offsetof`
  member designators, including nested struct/union paths and array
  elements (`offsetof(S, i.b)`, `offsetof(S, a[1].x)`).  Unknown paths and
  a final bitfield remain clear errors.
- `ffi.cdef` evaluates `sizeof("literal")`.  Escapes are decoded before
  the trailing NUL is counted, so static assertions, enum values and array
  sizes can use string-literal sizes.
- J2 runs subtract dot-product and subtract two-array division reductions
  (`s = s - a[i] * b[i]`, `s = s - a[i] / b[i]`) over raw tables and
  bounded cdata arrays.  `product - s` is a different expression and is
  rejected.
- J2 runs raw-table element stores `dst[i] = src[i] +|-|*|/ k` with
  constant, register and closure-upvalue scalars, including `ADDI`
  immediates, in-place stores, destination growth and descending or
  strided runs.

### Fixed

- J4 constant-string-key field stores now honor `__newindex` for an absent
  field instead of calling `luaH_set` raw.  Existing raw fields still
  bypass the handler, as in the interpreter.
- J4's `ADDI`/`FADDI` choice now emits an operand-subtype guard.  The old
  trace returned a float for an integer add or reinterpreted a double
  payload as an integer when the same Proto was entered with the other
  flavour.
- J4 rejects a constant `RK(C)` value in `OP_SETTABLE` instead of using
  the constant-table index as a stack slot.  A hot `t[i] = nil` therefore
  deletes the key instead of writing an unrelated local.
- J4's integer analyzer tracks table/string/boolean field reads and
  rejects table, string, userdata and boolean operands in raw integer
  arithmetic.  Table-valued fields and `__add`/`__sub` metatables on
  strings or userdata now call the metamethod instead of treating the
  value as an integer.
- The raw-table scaled store grows the destination to the run's maximum
  key, so descending runs do not write past a freshly grown array.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared` and `tools/upstream-check.sh`
are green.  New coverage: `tests/ffi/174_nested_offsetof.lua`,
`tests/ffi/175_sizeof_string.lua`,
`tests/ffi/176_j2_cdata_pair_sub.lua`, `tests/jit/193_pair_sub.lua`,
`tests/jit/194_table_scaled_store.lua`,
`tests/jit/195_j4_setfield_newindex.lua`,
`tests/jit/196_j4_addi_guard.lua`,
`tests/jit/197_j4_const_store.lua` and
`tests/jit/198_j4_arith_meta.lua`.

## [0.216.0] — 2026-11-24

### Added

- J2 runs weighted reductions over raw Lua tables and bounded cdata arrays
  in both the add and subtract spelling: `s = s +|- a[i] * k` and
  `s = s +|- a[i] / k`.  The scalar may be a numeric constant, a register
  local or a closure upvalue, and is re-resolved on every entry.  Division
  always produces a float, including for two integers and for a zero
  divisor (IEEE inf/NaN, never a Lua error); multiplication keeps Lua's
  integer wrap and float promotion.  The element and product/quotient
  temporaries keep their last bytecode values.
- J2 runs two-array division `s = s + a[i] / b[i]` over raw tables and
  bounded cdata arrays, joining the existing add/subtract/multiply
  reductions.  Division is left-to-right, so only the first read over the
  second is accepted.
- `luar.version_num` now encodes 0.216.0.  The minor component had lagged
  behind the release string.

### Fixed

- The affine raw-table fill intrinsic reused kind 4, the keyed-sum
  encoding.  A hot keyed sum therefore dispatched to the fill helper: it
  ran only the array-shaped prefix of the loop and left the accumulator
  unchanged, so `tests/jit/24`, `151` and `159` failed.  The fill now has
  its own kind, and an unknown intrinsic kind fails closed instead of
  falling through to the keyed sum.
- The three-table sum leaves the intermediate result and the second
  element register at their last bytecode values, and the keyed fill
  leaves the key temporary at the last key, instead of keeping the
  pre-loop value.  The direct keyed/upvalue matchers also reject a
  destination that is the visible loop-control register, which the
  interpreter would overwrite before `FORLOOP`.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan` and `tools/upstream-check.sh` are green.  New
coverage: `tests/jit/188_scaled_div.lua`,
`tests/jit/189_tint_kind_collision.lua`,
`tests/jit/190_scaled_sub.lua`, `tests/jit/191_pair_div.lua`,
`tests/ffi/171_j2_cdata_scaled.lua`,
`tests/ffi/172_j2_cdata_scaled_sub.lua`,
`tests/ffi/173_j2_cdata_pair_div.lua` and
`tests/jit/192_tint_writeback.lua`.

## [0.215.0] — 2026-11-23

### Fixed

- J4 (`jit.opt.start("ir=1")`) rejects mixed integer/float operand traces
  that it cannot represent faithfully, instead of letting the integer
  emitter reinterpret a double payload.  The FFI-operand shortcut that
  bypassed the post-floatification operand check is gone.  Mixed numeric
  `ADD`/`SUB` traces that are representable now use the runtime conversion
  helpers, which convert the integer operand without rewriting it: a
  source slot may be a visible local or a `FORLOOP` control register, and
  the old in-place `ITOF` insertion could change its value or subtype.
- K constants carry an explicit float tag.  A float `0.0` used with
  `ADDK`/`SUBK`/`MULK` was classified as integer because `0.0 == 0`, so
  the result was computed with the wrong semantics and tagged as an
  integer (`x + 0.0` returned an integer).  Such traces now stay on the
  interpreter/J2 path.  `DIVK` is recognized as always producing a float.
- `table.clear()` resets every cleared node's collision link.  Keys and
  values were reset, but a later insertion at the same main position
  inherited the old `next` offset, so collision walks could lose keys or
  loop forever.  The clear/refill stress case now completes and returns
  every key.

### Performance

- J2 runs `s = s - a[i]` over bounded cdata arrays through its reduction
  helper for 1/2/4-byte integers, 32-bit enums and 4/8-byte floats,
  including negative steps and mixed integer/float accumulators.
- J2 runs `s = s - t[i]` over a raw table array through a C helper, with
  integer/float promotion and negative steps.  Branch bodies keep using
  the generic emitter, whose subtraction behaviour is unchanged.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test` and
`make WITH_FFI=1 test-asan` are green.  New coverage:
`tests/jit/184_j4_mixed_ffi_operands.lua`,
`tests/jit/185_j4_kfloat_zero.lua`, `tests/jit/186_j2_table_sub.lua`,
`tests/ffi/169_j2_cdata_sub.lua` and
`tests/compat/table_clear_hash_chain.lua`.

## [0.214.0] — 2026-11-23

### Added

- `ffi.cdef` evaluates C's conditional operator `?:` in `#if`/`#elif`
  expressions.  It has C's precedence (below `||`), is right-associative,
  and only the selected branch is evaluated, so `#if 1 ? 1 : 1/0` is valid
  while a division by zero in the live branch remains an error.  `&&` and
  `||` now short-circuit in the same way.
- Array sizes and enum values accept the full C constant-expression
  operator set: comparisons, `&&`/`||` and `?:`, with the same lazy
  evaluation rule.  `enum { E = 1 ? 2 : 1/0 }` and
  `char a[(1 < 2) ? 4 : 5]` now parse.  Precedence follows C, and live
  division-by-zero and shift-count errors are unchanged.

### Fixed

- `ffi.cdef`'s `#if` parser no longer consumes the closing parenthesis of an
  enclosing expression after a bare `defined FOO`.  The common
  `#if (defined A || defined B)` form was rejected as unbalanced; the
  parenthesised `defined(A)` form still consumes its own `)` and now reports
  a clear error when it is missing.
- Backslash-newline continuations are spliced before directives, comments
  and declarations are parsed.  Multi-line `#define` bodies, multi-line
  `#if` expressions and `//` comments continued with a trailing backslash
  now behave as in C instead of leaving a backslash in the logical line.

### Performance

- J4 (`jit.opt.start("ir=1")`) compiles a loop that carries an integer
  reduction beside a double reduction, for example `s = s + a[i]` over an
  `int[]` while `x = x + d[i]` over a `double[]`, or the same shape over
  table arrays.  Such traces used to be classified as integer-only or
  rejected at the first integer cdata read, leaving the loop on the
  interpreter.  `j4/ffi_cdata_read` (1M elements, two accumulators) moved
  from 79.2 ms to 14.6 ms on the development host.  Pure-integer and
  pure-float traces keep their previous IR; a dynamic `GETTABLE` whose
  payload is a float is no longer eligible for integer-only emission.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test` and
`make WITH_FFI=1 test-asan` are green.  New coverage:
`tests/ffi/165_preproc_defined_group.lua`,
`tests/ffi/166_preproc_line_continuation.lua`,
`tests/ffi/167_preproc_ternary.lua`,
`tests/ffi/168_const_expr_operators.lua` and
`tests/jit/183_j4_mixed_reduction.lua`.

## [0.213.0] — 2026-11-23

### Fixed

- `ffi.cdef` evaluates `#if`/`#elif` operands with C's
  `intmax_t`/`uintmax_t` rules.  Literal type selection is suffix-aware:
  a hex/octal/binary literal above `INTmax_t` is unsigned
  (`0xffffffffffffffff > 0` is true, not a comparison against -1), a mixed
  signed/unsigned expression compares, divides and takes modulo as
  unsigned, and an unsigned right shift is logical.  Signed decimal
  literals, character constants and the existing shift-count/divide-by-zero
  diagnostics are unchanged.
- `ffi.new` rejects an aggregate or pointer cdata as a single array-element
  initializer.  The old path converted the source cdata's address to the
  element type and broadcast it over the destination, which silently
  produced garbage for `ffi.new("int[3]", other_array)` and copied the
  declared element count out of bounds when the source array was shorter.
  Numeric, boolean and enum scalar cdata still broadcast as documented.
- A bitfield read now has the same Lua-visible type as an ordinary field of
  its declared base type.  `_Bool` bitfields return booleans, 8-byte
  integers return cdata (so an unsigned all-ones field is
  `18446744073709551615`, not the Lua integer -1), and narrower integers and
  enums return Lua numbers.  This also makes union bitfields and bitfields
  at a non-zero bit offset consistent with their non-bitfield peers.
- `jit.off()` no longer consumes a loop's hot state.  Backedges executed
  while tracing was disabled previously reached the threshold and were
  recorded as already seen, so a later `jit.on()` left the loop on the
  interpreter until `jit.flush()`.  Disabled tracing now returns before
  touching the counters and `jit.on()` resumes compilation.

### Performance

- `table.clear()` clears the split array's contiguous value and tag runs
  with two `memset`s instead of a per-slot loop.  The table remains empty,
  capacity is retained, and array and hash references are still released.
  The `table/new_clear` benchmark best time moved from 2.399 ms to 2.265 ms
  on the development host (500k-entry array, repeat 15).

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test` and
`make WITH_FFI=1 test-asan` are green.  New coverage:
`tests/ffi/162_array_init_aggregate.lua`,
`tests/ffi/163_bitfield_read_types.lua`,
`tests/ffi/164_preproc_if_unsigned.lua`,
`tests/jit/182_jit_off_hot.lua` and
`tests/compat/table_clear_large.lua`; `tests/ffi/104_bool_conversions.lua`
now expects boolean results from `_Bool` bitfields.  The source diff is
tagged `/* LUAR */` where it edits the upstream core and the vendored tree is
untouched.

## [0.212.0] — 2026-11-23

### Added

- `ffi.cdef` supports C preprocessor `#elif` chains: the first true branch
  wins, later branch expressions are not evaluated, `#else` is taken only
  when no earlier branch was, and `#elif` without `#if`, `#elif` after
  `#else` and duplicate `#else` are clear errors.  The bodies of inactive
  groups are not parsed at all, so dead `#include`, `#define`, `#error`,
  malformed `#elif` expressions and unknown directives cannot fail an
  otherwise usable declaration.  Compact `#if(expr)`/`#elif(expr)` and
  comments directly after `#else`/`#endif` are accepted.
- `ffi.cdef` ignores non-pack `#pragma` directives (diagnostic and
  optimisation hints such as `#pragma once` or `#pragma GCC ...`) instead
  of rejecting headers that carry them; `#pragma pack(n/push/pop)` still
  controls layout.

### Fixed

- J4 (`jit.opt.start("ir=1")`) no longer records a float-controlled
  `FORLOOP` with its integer-count emitter.  The recorder rejects a loop
  whose counter/limit/step slots are not all integers, so a loop such as
  `for i = 1.0, n do s = s + 1 end` can no longer hang or wrap; J2 or the
  interpreter handles it.
- `jit.flush()` now clears a Proto's stale J4 root pointer together with
  its pc/start/base fields.  A flushed IR loop recompiles on its next hot
  backedge instead of being blocked by the already-unmapped trace.

### Performance

- With `ir=1`, backedges that J4 does not own (a `while`/`repeat` loop, a
  float-controlled `for`, or a rejected trace) now fall through to the J2
  emitter instead of staying interpreted.  A compiled J4 root keeps
  ownership of its own backedge.

### Tests

New coverage: `tests/ffi/160_preproc_elif.lua`,
`tests/ffi/161_pragma_unknown.lua`, `tests/jit/179_j4_float_for.lua`,
`tests/jit/180_j4_flush_recompile.lua` and
`tests/jit/181_ir_j2_fallback.lua`.

## [0.211.0] — 2026-11-23

### Added

- `ffi.cdef` accepts C integer literals in decimal, hexadecimal, octal
  (`010` is eight) and binary (`0b1010`) form, with `u`/`l` suffixes, and
  rejects malformed digits instead of silently truncating them.
- `ffi.cdef` accepts single-character C constants (`'A'`, `'\n'`,
  `'\x41'`, `'\101'`) in enum values and array sizes.  They follow plain
  `char` signedness like LuaJIT; empty, multi-character and unknown escapes
  are clear errors.
- Preprocessor `#if`/`#else` expressions now evaluate C integer operators
  (`! ~ + - * / % << >> < <= > >= == != & ^ | && ||`), parentheses,
  `defined` with or without parentheses, decimal/hex/octal/binary literals
  and character constants.  Undefined identifiers are zero, as in C, and
  malformed expressions/division by zero report the `#if` line.

### Fixed

- J2's fast `repeat ... until` path no longer compares an integer loop
  index against the zero immediate of a non-integral float constant.  A
  loop such as `repeat i = i - 1 until i == 3.5` was emitted as `i == 0`
  and terminated instead of running to the interpreter's infinite loop; the
  shape now stays on the interpreter.

### Performance

- A J2 intrinsic that declines at runtime (out-of-range cdata call,
  metatable table, short array) is suspended only for the current call
  generation, not permanently.  Later calls of the same compiled loop retry
  the intrinsic, so one transient out-of-range input no longer downgrades a
  native loop to interpreted code.  A permanently unsuitable shape pays one
  helper attempt per call rather than one per backedge.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make WITH_FFI=1 test-fuzz`,
`make test-shared`, `make test-dist`, `make test-upstream`,
`make dynasm-check` and `tools/upstream-check.sh` are green.  New coverage:
`tests/ffi/156_int_literal_bases.lua`,
`tests/ffi/157_char_constants.lua`,
`tests/ffi/158_preproc_if_expr.lua`,
`tests/ffi/159_j2_tint_retry.lua` and
`tests/jit/178_repeat_float_k.lua`; `tests/ffi/16_preproc.lua` now checks
that a full `#if` expression is evaluated.

## [0.210.0] — 2026-11-24

### Added

- J2 updates bounded cdata arrays held in closure upvalues in place:
  `a[i] = a[i] +|-|*|/|//|% v`, the bitwise family, shifts and unary
  `~`/`-`, with register, immediate and constant scalar operands.  The
  upvalue is re-resolved on every native entry, the GETUPVAL destination
  and arithmetic temporary are preserved, and a rebinding to a non-array
  declines so the interpreter raises the exact error.
- J2 reduces and updates enum-element cdata arrays.  Elements use the
  interpreter's unsigned 32-bit numeric view, including negative enum
  constants and values with the high bit set, and the store helpers apply
  the same `CT_ENUM` conversion as `ffi_convert`.
- J2 stores into store-only `_Bool` cdata arrays.  Pure fills, value
  slots, copies, source-scalar stores and two-source stores normalize any
  non-zero scalar to 1 (C `_Bool` semantics) instead of writing the raw
  low byte; bool element reads stay on the interpreter because they yield
  Lua booleans.

### Fixed

- A metatype `__new` handler is now dispatched when the CType object is
  called (`ct(...)`), matching LuaJIT.  The handler receives the type
  object and the original arguments, and all its results become the
  call's results.  `ffi.new`/`ffi.cast` keep the built-in constructor.
- `ffi.new` accepts a Lua string only for a 1-byte character pointer.
  `void *` and other pointer destinations now reject the initializer, as
  LuaJIT does, instead of silently reinterpreting the string bytes.
  `ffi.cast` and string arguments in calls remain permissive.
- J4 pointer-field reads/stores only accept the widths their helpers can
  actually address: 4/8-byte integers and 8-byte doubles.  `char *`,
  `short *` and `float *` fields previously used the wrong stride, which
  could return wrong values or overwrite neighbouring elements; they now
  stay on the interpreter.
- J4 scalar C-call returns preserve the interpreter's result type and
  width: 64-bit integer results stay cdata and the loop is interpreted,
  narrow signed/unsigned integers are extended according to the declared
  type, and `float`-returning `void` calls are read as 4-byte floats rather
  than doubles.
- CType objects no longer compare equal to an ordinary cdata value of the
  denoted type (`ffi.typeof("int") == ffi.new("int")` is false in both
  orders, as in LuaJIT); two CType objects of the same interned type still
  compare equal.
- The arm64 non-HFA large-union by-value argument override is no longer
  cleared immediately, restoring AAPCS64 by-reference passing.
- `jit.opt.start("hotloop=0")` now disables tracing for newly created
  Protos instead of silently keeping the default threshold.
- J4's slot-writer analysis now accounts for every IR op that writes a
  result slot, preventing a stale constant from being substituted for a
  later table/call result.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared`, `make test-dist`,
`make dynasm-check` and `tools/upstream-check.sh` are green.  New
coverage: `tests/ffi/148_metatype_new.lua`,
`tests/ffi/149_new_pointer_string.lua`,
`tests/ffi/150_j2_enum_array.lua`, `tests/ffi/151_j2_bool_fill.lua`,
`tests/ffi/152_j2_cdata_upval_update.lua`,
`tests/ffi/153_j4_call_widths.lua`,
`tests/ffi/154_j4_ptr_field_widths.lua`,
`tests/ffi/155_ctype_equality.lua` and `tests/jit/177_hotloop_zero.lua`.

## [0.209.0] — 2026-11-24

### Added

- J2 sums bounded cdata arrays captured as closure upvalues
  (`s = s + a[i]`).  The upvalue is re-resolved on every native entry, so
  rebinding it to another array is observed like the interpreter; strided
  and descending loops use the same FORLOOP key sequence as the register
  form.
- J2 fills cdata arrays held in closure upvalues (`a[i] = v`, `a[i] = i`
  and numeric constants), including 8-byte integer destinations.  The
  upvalue is re-resolved per entry and the GETUPVAL temporary keeps the
  live array after the run.
- J2 stores into 8-byte integer cdata destinations for element-wise copy,
  source-scalar arithmetic and two-source arithmetic.  These are pure store
  contexts: the helper applies the FFI C-store conversion and never loads
  the destination element into a Lua slot, so int64_t/uint64_t arrays no
  longer need to stay interpreted for writes.
- J2 updates integer cdata arrays in place with `a[i] = a[i] & v`, `|`,
  `~`, `<<`, `>>`, their immediate forms and unary `~a[i]`/`-a[i]`.  Lua's
  integer conversion, two's-complement wrap and shift rules are preserved
  (negative counts shift the other way, `|count| >= 64` yields zero), and a
  non-integral operand declines before the first store so the interpreter
  raises the exact error.
- J2 sums a raw table array reached through a constant table field
  (`s = s + obj.arr[i]`).  The field is re-read and validated on every
  native entry, so replacing it, shrinking the array or making it missing
  falls back to the interpreter; the GETFIELD/GETTABLE temporary keeps the
  last element.

### Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared`, `make test-dist`,
`tools/upstream-check.sh` and `make dynasm-check` are green.  New
coverage: `tests/ffi/144_j2_cdata_upval_sum.lua`,
`tests/ffi/145_j2_cdata_i64_store.lua`,
`tests/ffi/146_j2_cdata_bitwise_update.lua`,
`tests/ffi/147_j2_cdata_upval_fill.lua` and
`tests/jit/176_field_table_sum.lua`.


## [0.208.0] — 2026-11-24

### Added

- J2 stores bounded cdata arrays with floor division and modulo:
  `a[i] = a[i] // v`, `a[i] = a[i] % v`, `a[i] = b[i] // v`,
  `a[i] = b[i] % v`, `a[i] = b[i] // c[i]` and `a[i] = b[i] % c[i]`,
  including register, immediate and constant scalar operands for the
  two-array forms.  The helpers follow Lua's negative-operand floor
  semantics and int wrap, keep the bytecode-visible temporaries, and
  decline an integer zero divisor before storing so the interpreter raises
  the exact `n//0`/`n%0` error.  Float zero follows the non-raising
  inf/NaN path.

### Fixed

- J4 side traces now always run the float/integer analysis.  Previously an
  integer-only root skipped it, so a side body with a float-constant
  `ADDK`/`SUBK`/`MULK`/`MODK`/`IDIVK` and no float guard was emitted with
  the integer constant 0 and retagged as an integer.  Unsupported float K
  ops now fall back to the interpreter; `MULK`/`DIVK` still specialize to
  their float forms.  The IR verifier also rejects unsupported float K
  constants before emission.
- The IR recorder initializes its per-instruction emit flag and marks the
  pending int64/field-chain paths explicitly.  A nested cdata field chain
  could previously consult an uninitialized value; the same initialization
  also keeps the int64 array fusion paths from depending on stack garbage.
- `ffi.copy` validates its source even when the requested length is zero.
  Numbers, tables, booleans and scalar cdata were silently accepted because
  the function returned before the source check; only a NULL (`nil`)
  zero-length source is accepted now.

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
