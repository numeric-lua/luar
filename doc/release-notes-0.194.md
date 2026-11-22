# Luar 0.194 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds two J2 numeric-loop families, fixes two JIT correctness bugs and closes
a nine-byte wide-bitfield overrun in the FFI.

## Highlights

- J2 compiles integer shifts whose count is held in a register
  (`OP_SHL`/`OP_SHR`).  Negative counts shift the other way, `|count| >= 64`
  yields zero, and `SHR` stays logical.  Several variable shifts may appear
  in one loop.
- J2 compiles unary minus for integer and float loop values.  Integer
  negation wraps at `INT64_MIN`; float negation flips the IEEE sign bit, so
  `-0.0` and NaN signs match the interpreter bit for bit.
- A mixed integer/float comparison guard now records the originating
  bytecode PC correctly.  An exit resumes at the comparison instead of
  restarting the function and repeating side effects before the loop.
- The specialised `repeat` emitter only matches a pure body.  An extra
  assignment or comparison used to be accepted and silently dropped from
  the generated loop.
- A full-width 64-bit FFI bitfield at a non-zero bit offset spans nine bytes
  (for example `unsigned char a:1; unsigned long long b:64;`).  Load and
  store now use a two-word path, so they no longer read or write past the
  field; ASan previously reported a stack-buffer overflow on the store.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make WITH_FFI=1 test-asan
make dynasm-check
tools/upstream-check.sh
```

New tests:

- `tests/jit/132_repeat_extra_ops.lua`
- `tests/jit/133_exit_pc_side_effect.lua`
- `tests/jit/134_variable_shifts.lua`
- `tests/jit/135_unary_minus.lua`
- `tests/ffi/103_wide_bitfield_span.lua`

## Known limitations

- J2 still compiles only loop-level numeric code; calls, table
  `__index`/`__newindex` and generic traces remain on the interpreter or the
  opt-in J4 tier.
- Variable shifts are compiled only for integer operands; a float operand
  falls back to the interpreter.
- The wide-bitfield fix covers the little-endian access path used by the
  tested x86-64 target.
