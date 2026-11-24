# Luar 0.214 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
hardens `ffi.cdef`'s C-expression handling and lets J4 compile mixed
integer/double reductions.

## Highlights

- `ffi.cdef` evaluates the conditional operator `?:` in `#if`/`#elif`.
  Precedence, right associativity and lazy branch evaluation follow C, so
  `#if 1 ? 1 : 1/0` is accepted and a live `1 / 0` is still diagnosed.
  `&&` and `||` short-circuit as well.
- Array sizes and enum values support comparisons, `&&`/`||` and `?:`.
  Both declaration constants and preprocessor expressions now share the
  same operator surface; `enum { E = 1 ? 2 : 1/0 }` is valid.
- A bare `defined FOO` no longer swallows the `)` that closes an enclosing
  `#if` expression.  `#if (defined A || defined B)` is accepted, while
  `defined(A` reports the missing parenthesis.
- Backslash-newline continuations are spliced before the preprocessor
  tokenises a logical line.  Multi-line macros, multi-line `#if`
  expressions and continued `//` comments now behave as in C.

## Performance

J4 (`jit.opt.start("ir=1")`) used to reject or misclassify a loop with an
integer accumulator over `int[]` beside a double accumulator over
`double[]` (and the equivalent table shape).  One trace now carries both
chains, each with its own guards.  On the development host the
`j4/ffi_cdata_read` benchmark, which reads one million `int` and one million
`double` cdata elements, moved from 79.2 ms to 14.6 ms.  Pure-integer and
pure-float reductions keep their existing IR, and a dynamic `GETTABLE` that
yields a float is refused by the integer-only path.

## Correctness notes

- Only the selected branch of `?:` and the necessary operand of `&&`/`||`
  is evaluated.  Syntax and type errors in a dead branch still surface, as
  they do in a C compiler.
- A live division by zero, invalid shift count, malformed literal or
  unbalanced parenthesis is still a clear `ffi.cdef` error.
- The declaration constant-expression domain remains signed 64-bit; the
  preprocessor retains its C `intmax_t`/`uintmax_t` rules from 0.213.

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

- `tests/ffi/165_preproc_defined_group.lua`
- `tests/ffi/166_preproc_line_continuation.lua`
- `tests/ffi/167_preproc_ternary.lua`
- `tests/ffi/168_const_expr_operators.lua`
- `tests/jit/183_j4_mixed_reduction.lua`

## Known limitations

- `ffi.cdef` still does not parse arbitrary system headers: compiler
  include directories such as GCC's `stddef.h` are not searched, and
  attributes outside the documented subset are rejected.
- The preprocessor handles `\` immediately before `\n`; a lone trailing
  backslash at end of file is left literal.
- J4 roots only integer-controlled `for` loops.  Float-controlled loops and
  unsupported shapes continue to use J2 or the interpreter.
