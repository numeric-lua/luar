# Luar 0.204 release notes

This release hardens FFI numeric and pointer conversions, fixes a silent
J4 miscompile of 64-bit struct fields, and makes the default J2 tier run
cdata array reductions natively.  FFI function symbols are also cached per
namespace, matching LuaJIT's cdata identity and removing repeated `dlsym`.

## JIT

* J2 recognises `for i = 0, n - 1 do s = s + a[i] end` when `a` is a
  bounded cdata array of 1/2/4-byte integers or 4/8-byte floats.  A
  bounds-checked C helper runs the remaining FORLOOP iterations, preserving
  int64 wrap, unsigned widening, int-to-float promotion, the post-loop
  element temporary and the control slots.  This works in the default tier
  (no `ir=1`), and `jit.stats().tint_hits` observes it.  Pointer sources
  have no length to validate and stay on the interpreter.
* J4 no longer records 8-byte integer struct/union fields.  A `long` or
  `int64_t` member read produces cdata int64, not a Lua integer; treating
  it as an integer could silently return a pointer-like value for
  `x = x + s.z` once the loop became hot.  The field recorder now rejects
  those members and the interpreter remains authoritative.  Narrower
  integer fields keep tracing.

## FFI

* Out-of-range double-to-integer conversions are defined and match
  LuaJIT's observable x86-64 behaviour instead of inheriting C undefined
  behaviour: signed overflow and NaN/infinity use the `INT64_MIN` sentinel,
  `[2^63, 2^64)` is converted exactly, `2^64` wraps to zero, and the
  high-range subtraction behaviour is preserved.  This affects
  `ffi.new`/`ffi.cast`, array and struct stores, cdata arithmetic and
  numeric comparisons.
* `ffi.new` pointer initializers follow LuaJIT's type rules.  Numbers,
  numeric cdata and incompatible pointers are rejected; `void *` still
  interoperates with object pointers, arrays decay to a matching pointer,
  and an aggregate can initialize a pointer to its exact type.  The
  existing `char *` string convenience is preserved, and `ffi.cast`
  remains a permissive C cast.
* Function symbols are cached in each `ffi.C`/`ffi.load` namespace.
  Repeated `ffi.C.fn` accesses now return the same cdata object
  (`rawequal` is true, as in LuaJIT), avoid a repeated `dlsym` and userdata
  allocation, and keep the library alive for the cached pointer.  Variables
  and constants remain per-access snapshots.

## Performance microchecks

On the development host (Ryzen 3 7320U, gcc 16, `-O3`), a 2M-element
`ffi.new("double[?]")` sum in the default J2 tier went from 239 ms
interpreted to 2.2 ms native; the `int[?]` form went from 249 ms to 2.1 ms
after the element loads were made constant-size.  Two million repeated
`ffi.C.abs` lookups dropped from 960 ms to 476 ms with the symbol cache.
These are microbenchmarks, not application claims or release gates.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `make test-shared`, `make test-dist`,
`tests/upstream/run.sh` and `tools/upstream-check.sh` are green.  New
coverage: `tests/ffi/120_float_int_conversion.lua`,
`tests/ffi/121_j2_cdata_sum.lua`,
`tests/ffi/122_int64_field_no_trace.lua`,
`tests/ffi/123_new_pointer_strict.lua` and
`tests/ffi/124_symbol_cache.lua`.
