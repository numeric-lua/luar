# Luar 0.211 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the FFI declaration parser, fixes a J2 `repeat` miscompile and
keeps native table/cdata intrinsics available after a transient refusal.

## Highlights

- `ffi.cdef` now parses C integer literals in all four bases.  A leading
  zero means octal (`010` is eight), `0b1010` is ten, and invalid digits
  such as `09` or `0b102` are rejected with a clear parser error.
- Character constants (`'A'`, `'\n'`, `'\x41'`, `'\101'`) are accepted in
  enum values and array sizes.  A single character is sign-extended through
  plain `char`, matching LuaJIT (`'\xff'` is -1 on the tested targets).
  Empty, multi-character and unknown-escape constants are errors.
- `#if` and `#else` now evaluate real C integer constant expressions:
  boolean, comparison, arithmetic, shift and bitwise operators, parentheses,
  `defined X`/`defined(X)`, octal/hex/binary/character constants, and
  undefined identifiers as zero.  This makes normal header guards such as
  `#if defined(FEATURE) && FEATURE > 0` work without hand-expanding them.

## Correctness fixes

- J2's fast `repeat ... until` emitter only had an integer immediate for a
  constant comparison.  A non-integral float constant (`until i == 3.5`)
  was emitted as `i == 0`, so a loop that should never terminate returned
  early.  The emitter now rejects a float `OP_EQK` immediate and leaves the
  loop to the interpreter.

## Performance

- J2 table/cdata intrinsics are suspended per call generation instead of
  being discarded after the first runtime refusal.  An out-of-range cdata
  call, a metatable table or a short array no longer permanently disables a
  loop that is native for normal inputs; the next call retries the helper.
  A shape the intrinsic can never handle pays one cheap helper attempt per
  call rather than one per backedge.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make WITH_FFI=1 test-fuzz
make test-shared
make test-dist
make test-upstream
make dynasm-check
tools/upstream-check.sh
```

New tests:

- `tests/ffi/156_int_literal_bases.lua`
- `tests/ffi/157_char_constants.lua`
- `tests/ffi/158_preproc_if_expr.lua`
- `tests/ffi/159_j2_tint_retry.lua`
- `tests/jit/178_repeat_float_k.lua`

`tests/ffi/16_preproc.lua` was updated because a full `#if` expression is
now evaluated rather than rejected.

## Known limitations

- `#elif` is still not implemented; use nested `#if`/`#else`.
- Complex `#if` operands with unsigned 64-bit suffix semantics are
  evaluated in the parser's signed 64-bit integer domain, the same domain
  the declaration constant parser uses.
- The `repeat ... until <float constant>` shape now stays interpreted; it
  has no terminating integer solution, so this is a correctness fallback
  rather than a performance target.
