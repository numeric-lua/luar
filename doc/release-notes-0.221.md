# Luar 0.221 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends cdata store coverage to closure-upvalue sources and scalars.

## Highlights

- Cdata scalar stores accept an upvalue scalar:
  `a[i] = b[i] +|-|*|/|//|%|&|~|<<|>> k` where `k` is a closure upvalue.
  The upvalue is re-read on every entry, so rebinding it or changing an
  integer value is observed.
- Cdata stores accept an upvalue source array for scalar stores
  (`a[i] = U[i] OP k`), two-source stores (`a[i] = U[i] OP V[i]`), unary
  stores (`a[i] = -U[i]`, `~U[i]`) and copies (`a[i] = U[i]`).
- Mixed register/upvalue sources and in-place destinations are covered.
  A GETTABLE destination may alias the upvalue-load temporary because the
  upvalue is reloaded on the next iteration.

## Correctness notes

- The scalar encoding is disambiguated: register, immediate, Proto
  constant and closure upvalue each have a distinct form.
- A changed upvalue value is observed; string or other non-numeric
  upvalues fall back to the interpreter and keep its error.
- Integer bitwise forms still require integer elements; float operands
  remain interpreted.

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

- `tests/ffi/184_j2_cdata_upval_scalar.lua`
- `tests/ffi/185_j2_cdata_upval_source.lua`

## Known limitations

- The upvalue forms still cover bounded cdata arrays; pointer sources and
  8-byte integer element reads remain interpreter-side.
- Verification is Linux x86-64 only; no macOS/arm64 or CI run in this
  cycle.
