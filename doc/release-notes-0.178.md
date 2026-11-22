# Luar 0.178 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds a cdata integer-width audit.

## Highlights

- `tests/ffi/82_cdata_int_widths.lua` compares JIT and interpreter results
  for `int8_t`, `int16_t`, `int32_t` and `uint32_t` arrays with negative and
  high-bit values.
- Follows the 0.177 fix that removed size-8 integer arrays from the 32-bit
  `CGETI` path.
- No runtime behavior change.

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
