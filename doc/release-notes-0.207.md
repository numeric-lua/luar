# Luar 0.207 release notes

This release broadens the default J2 tier's cdata write coverage, caches
parsed C type names, and fixes `%q` formatting for non-numeric cdata.

## JIT

* J2 runs scaled bounded cdata stores: `a[i] = b[i] + v`, `- v`, `* v`
  and `/ v`, with a register, immediate or constant scalar.  The helper
  reproduces Lua's int-wrap and int-to-float promotion before applying the
  FFI element-store conversion, and leaves the last computed result in the
  bytecode temporary.
* J2 runs two-source cdata division, `a[i] = b[i] / c[i]`, joining the
  existing add/subtract/multiply pair stores.  Division is always float in
  Lua before the destination store conversion.
* J2 fills 8-byte integer cdata arrays with `a[i] = v|i|constant`
  (`int64_t` and `uint64_t`), including the out-of-range double sentinel
  and unsigned wraparound.  Copy/update/pair forms remain interpreted
  because an 8-byte element read is cdata int64/uint64, not a Lua number.
* All three families are visible through `jit.stats().tint_hits` and use
  the existing nonblocking fallback rule for rejected shapes.

## FFI

* Type names parsed from strings are cached per Lua state, capped at 1024
  distinct spellings.  `ffi.new`, `ffi.cast`, `ffi.sizeof` and
  `ffi.typeof` avoid reparsing a declaration that was already interned.
  Only successful parses are cached, so a later `ffi.cdef` can still
  define a missing typedef or complete a forward struct.
* `string.format("%q", cdata)` formats pointers, arrays, aggregates and
  CType objects through `tostring`, matching LuaJIT.  Numeric cdata
  conversions and `%s` are unchanged.

## Performance

Measured on the development host with `os.clock`, J2 versus the
interpreter, on 20M-element arrays:

| case | interpreted | native | speedup |
| --- | ---: | ---: | ---: |
| `int64_t` fill (`a[i] = 7`) | 3520 ms | 74 ms | 48x |
| `uint64_t` fill (`a[i] = -1`) | 3477 ms | 78 ms | 45x |
| `d[i] = b[i] * 2.5` | 6394 ms | 84 ms | 76x |
| `d[i] = b[i] / c[i]` | 9692 ms | 105 ms | 92x |
| `int32_t` `d[i] = b[i] + 3` | 6659 ms | 117 ms | 57x |

A 200k-call type lookup loop measured cached `ffi.sizeof("int[1]")` at
62 ms versus 945 ms when each spelling was new (15x); `ffi.typeof` was
64 ms versus 954 ms.  These are shared-host microchecks, not release gates.

## Tests

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`,
`make WITH_FFI=1 test-asan`, `tools/upstream-check.sh` and
`make dynasm-check` are green.  New coverage:

* `tests/ffi/135_j2_cdata_scalar_store.lua`
* `tests/ffi/136_j2_cdata_pair_div.lua`
* `tests/ffi/137_j2_cdata_i64_fill.lua`
* `tests/ffi/138_ctype_string_cache.lua`
* `tests/ffi/139_format_q_cdata.lua`
