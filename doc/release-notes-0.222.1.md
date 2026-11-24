# Luar 0.222.1 release notes

This is a correctness patch for three J4 tracing bugs.

## Fixes

- **Float-controlled loop reusing an integer J4 trace.**  The integer
  control triplet is now checked when the IR is entered, not only while
  recording.  A loop whose init/limit/step are floats falls back instead
  of treating the double payloads as counts, which previously produced a
  wrong trip count or a hang.
- **Stale operand type after `SLOAD`.**  `SLOAD` now clears the recorded
  `seen` state and the type-analysis tag for its destination.  A later
  `ADD`/`ADDK`/`SUBK`/`MULK` guards the live value, so table operands call
  `__add`/`__mul` instead of adding raw table-address bits.
- **Suppressed cdata `CGUARD` after `SLOAD`.**  The same invalidation
  re-emits the cdata identity guard before a field access.  Reading or
  writing a different struct type that shares the USERDATA tag can no
  longer reuse a previous type's offset and overwrite an adjacent field.

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

New regression tests:

- `tests/jit/215_j4_forloop_float_reuse.lua`
- `tests/jit/216_j4_sload_stale_tag.lua`
- `tests/ffi/188_j4_cdata_cguard_sload.lua`

## Known limitations

- J4 still roots only integer-controlled numeric for-loops; float control
  deliberately falls back to J2/interpreter.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
