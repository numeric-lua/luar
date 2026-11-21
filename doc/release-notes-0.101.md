# Luar 0.101 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes a float add-immediate miscompile.

## Highlights

- `v + 1` with a float `v` previously emitted integer `ADDI`, adding the
  immediate to the float bit pattern.  It now emits `FADDI` and the
  integer analyzer rejects that op.
- `t[ki] = v + 1` now produces a float value (`3.5`), not a huge integer.
- `tests/jit/101_trace_float_addi.lua` locks down the value and
  `math.type`.
- The float loop still falls back when the combined trace cannot install;
  correctness is the shipped fix.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```

## Known limitations

- Integer/float dynamic-key writes not covered by the narrow TSETDYN path
  remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
