# Luar 0.69 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
generalizes upvalue loading beyond C functions.

## Highlights

- **Numeric/table upvalues**: `ULOAD` copies the value under a closure
  identity check, then a tag guard protects downstream typed operations.
  Changing the upvalue value of the same type is visible; a type change
  exits to the interpreter.
- Function/cdata upvalues keep the identity-only path used by native calls.
- `tests/jit/79_trace_upvalue.lua` covers value changes, type changes and
  two closures of the same Proto.

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

- Table upvalues used as the target of TSET/TGET still do not record
  (closing over a mutable table remains interpreter-side).
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
