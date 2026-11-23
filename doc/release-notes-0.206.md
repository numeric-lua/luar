# Luar 0.206 release notes

This release extends the default J2 tier from cdata reads to cdata writes,
and closes a LuaJIT pointer-cast compatibility gap.

## JIT

* J2 runs bounded cdata array stores in the default tier:
  `a[i] = v`, `a[i] = i` and `a[i] = constant` for 1/2/4-byte integer and
  4/8-byte float elements.  The helper follows the interpreter's FORLOOP
  key sequence for any nonzero integer step, applies the FFI store
  conversions and LuaJIT's out-of-range double-to-integer sentinel, and
  leaves pointers and out-of-bounds runs on the interpreter.
* J2 copies bounded cdata arrays with `a[i] = b[i]`, converting between
  integer widths and float/double element types per store while preserving
  the source-element temporary.
* J2 updates bounded cdata arrays in place with
  `a[i] = a[i] +|-|*|/ v`, including register, immediate and constant
  scalar operands.  Lua's int wrap and int-to-float promotion happen before
  the C store conversion.
* J2 stores two-source cdata results with `a[i] = b[i] +|-|* c[i]`,
  converting the computed value to the destination element type and
  preserving both source temporaries.
* All four store families are visible through `jit.stats().tint_hits`.
  They use the same nonblocking fallback rule as the reduction
  intrinsics: a rejected shape is dropped for the current epoch instead of
  being retried at every backedge.

## FFI

* `ffi.cast` accepts a Lua string for any pointer or function-pointer type,
  matching LuaJIT (`ffi.cast("int *", "abc")`,
  `ffi.cast("int (*)(void)", "abc")`).  `ffi.new` keeps its stricter
  pointer-initializer rules, and the string is not anchored by the cdata,
  as documented.

## Performance

A 2M-element `int32_t[?]` microcheck on the development host measured fill
288 ms interpreted versus 7 ms native, element copy 528 ms versus 8 ms,
two-source add 804 ms versus 11 ms, and in-place multiply 613 ms versus
13 ms.  These are `os.clock` microchecks on a shared laptop-class host, not
release gates.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tools/upstream-check.sh` and
`make dynasm-check` are green.  New coverage:

* `tests/ffi/130_j2_cdata_store.lua`
* `tests/ffi/131_j2_cdata_copy.lua`
* `tests/ffi/132_j2_cdata_update.lua`
* `tests/ffi/133_cast_string_pointer.lua`
* `tests/ffi/134_j2_cdata_pair_store.lua`
