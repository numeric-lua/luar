# Luar 0.212 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends the FFI declaration preprocessor and closes three J4 correctness and
fallback gaps.

## Highlights

- `ffi.cdef` now implements C preprocessor `#elif` chains.  The first true
  branch wins, later branch expressions are not evaluated, and `#else` is
  taken only when no earlier branch was.  Structural mistakes (`#elif`
  without `#if`, `#elif` after `#else`, duplicate `#else`) stay clear errors.
- Bodies of inactive conditional groups are no longer parsed.  Dead
  `#include`, `#define`, `#error`, malformed `#elif` expressions and unknown
  directives cannot make an otherwise valid declaration fail.  Only the
  conditional nesting is tracked through a skipped group.
- Non-pack `#pragma` directives are ignored.  Headers carrying
  `#pragma once`, `#pragma GCC diagnostic ...`, `#pragma clang ...`,
  `#pragma message` or vendor pragmas now work without hand-editing;
  `#pragma pack(n/push/pop)` still controls layout exactly as before.
- Compact `#if(expr)`/`#elif(expr)` and comments immediately after
  `#else`/`#endif` are recognised, so skipped-group nesting stays correct
  for those valid C spellings.

## Correctness fixes

- J4 (`jit.opt.start("ir=1")`) no longer records a float-controlled
  `FORLOOP` with its integer-count emitter.  The recorder now requires the
  counter, limit and step slots to be integers.  A loop such as
  `for i = 1.0, n do s = s + 1 end` previously treated the double payloads
  as an integer count/index and could hang or wrap; it now stays on J2 or
  the interpreter.
- `jit.flush()` now clears a Proto's stale J4 root pointer together with
  its pc/start/base fields.  The hot hook used to see the unmapped pointer
  as an installed trace and skip re-recording, so a flushed IR root never
  came back.  It now recompiles on the next hot backedge.

## Performance

- With `ir=1`, a backedge that J4 does not own now falls through to the J2
  emitter instead of staying interpreted.  This covers `while`/`repeat`
  loops, float-controlled `for` loops and traces J4 rejects.  A successfully
  installed J4 root keeps ownership of its own backedge.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make WITH_FFI=1 test-fuzz
make test-shared
make test-dist
make dynasm-check
tools/upstream-check.sh
```

New tests:

- `tests/ffi/160_preproc_elif.lua`
- `tests/ffi/161_pragma_unknown.lua`
- `tests/jit/179_j4_float_for.lua`
- `tests/jit/180_j4_flush_recompile.lua`
- `tests/jit/181_ir_j2_fallback.lua`

## Known limitations

- The preprocessor ignores non-pack pragmas rather than interpreting them; a
  pragma that changes layout must be expressed as `#pragma pack` or an
  attribute the parser already supports.
- `#if`/`#elif` expressions are evaluated in the parser's signed 64-bit
  integer domain, the same domain used by declaration constants.  Complex
  operands relying on unsigned 64-bit suffix semantics still need
  hand-expansion.
- J4 roots only integer-controlled `for` loops.  Float-controlled loops and
  other shapes continue to use J2 or the interpreter.
- J4 does not trace 64-bit cdata struct/union fields because a field read
  yields cdata int64 rather than a Lua integer.
