# Luar 0.105 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends list-constructor tracing to 200 elements.

## Highlights

- The FORLOOP `Bx` precheck fix also unblocks 200-element constructors.
- `tests/jit/102_trace_large_list.lua` covers a 200-element list, forced GC
  and `alloc=0`.
- No new IR was needed; the existing 1024-op budget suffices.

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

- Larger constructors may still hit frame/slot limits.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
