# Luar 0.235 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds raw 64-bit cdata in-place updates with cdata scalars.

## Highlights

- `a[i] = a[i] +|-|*|&|||~|<<|>> k` over bounded int64/uint64 arrays uses
  raw 8-byte arithmetic.
- Shift counts use the cdata mask-to-six-bits rule.
- The visible element temporary is a cdata scalar; uint64 destinations and
  mixed scalar types are covered.

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

New test:

- `tests/ffi/196_j2_i64_update.lua`

## Known limitations

- Division/modulo updates with cdata scalars remain interpreter-side.
- Bounded 8-byte arrays only; pointer arrays fall back.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
