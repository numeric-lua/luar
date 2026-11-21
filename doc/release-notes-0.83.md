# Luar 0.83 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds two-string concatenation to J4 traces.

## Highlights

- `a .. b` lowers to `CONCAT2` for local string operands; the helper
  allocates the result with `luaS_newlstr` and writes it to the destination.
- Record-time string MOVEs skip the generic tag guard; the concat helper
  validates both operands at runtime and exits cleanly if they are not
  strings.
- `tests/jit/87_trace_concat.lua` covers tracing, forced GC and `alloc=0`.

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

- Only exactly two string operands are supported; longer CONCAT chains and
  numbers in concatenation fall back.
- The result is not sunk; each iteration allocates a string.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
