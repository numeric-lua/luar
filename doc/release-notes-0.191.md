# Luar 0.191 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
hardens the fused int64 FFI tier and extends it with shifts and unary
operators.

## Highlights

- Fused int64 operations keep full 64-bit constants.  Before this release
  `a[i] = a[i] * 4294967296` truncated the factor to `int`, and constants
  above `2^53` lost precision through a `double`.
- `uint64_t` arrays use unsigned fused `//` and `%`; high-bit values no
  longer differed between the interpreter prologue and the native loop.
- Fused `INT64_MIN / -1` and `INT64_MIN % -1` match the interpreter
  (`INT64_MIN` and `0`) instead of raising `SIGFPE` through x86 `idiv`.
  Fused add and multiply use wrap-defined unsigned arithmetic, removing C
  signed-overflow undefined behaviour.
- New `CSHLI64`/`CSHRI64`: in-place `a[i] = a[i] << n` and `>> n` run
  natively with cdata's logical shift and low-six-bit count rules.
- New `CNEGI64`/`CBNOTI64`: in-place `a[i] = -a[i]` and `a[i] = ~a[i]`.
- `tests/ffi/95_int64_constants.lua` through `tests/ffi/99_int64_unary.lua`
  cover the new behaviour, including forced GC (`alloc=0`) and interpreter
  fallback (`ir=0`).

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
