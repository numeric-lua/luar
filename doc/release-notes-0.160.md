# Luar 0.160 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds integer-constant widening for `(float,float)->float` native calls.

## Highlights

- `hypotf(3, 4)` now compiles and executes natively.
- The recorder reads the `LOADI` constants from its own IR and rewrites
  the two argument slots to `LOADF`, avoiding runtime conversion and stale
  stack slots.
- `tests/ffi/65_trace_int_widen_2ff.lua` checks execution, zero entry
  failures and `alloc=0`.
- Integer variables (not compile-time `LOADI`) still fall back.

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
