# Luar 0.104 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes long loop-body recording.

## Highlights

- The recorder’s generic `b >= ns` precheck now exempts `FORLOOP`, whose
  `Bx` field is a jump offset, not a register.  A 220-statement loop body
  (441 bytecode instructions) now records and executes.
- `LUAR_JIT_MAXINS` rises from 192 to 1024 for the loop snapshot.
- `tests/jit/103_trace_long_loop.lua` covers the long body.

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

- 200-element constructors still depend on other slot/recorder limits.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
