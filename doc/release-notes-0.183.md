# Luar 0.183 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a raw int64 cdata array copy path.

## Highlights

- `dst[i] = src[i]` over `int64_t` arrays compiles and executes natively.
- The recorder keeps a pending int64 source across `GETTABLE`/`SETTABLE`
  and emits `CCPYI64`; a helper copies 8 bytes without boxing cdata.
- ULOAD known-value detection fixes the first-pass classifier miss.
- `tests/ffi/85_int64_array_copy.lua` checks compilation, execution, every
  element, `alloc=0` and interpreter fallback.

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
