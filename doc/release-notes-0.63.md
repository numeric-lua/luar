# Luar 0.63 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
makes TSET growth safe if GC relocates the Lua stack.

## Highlights

- **Relocated base**: `luar_trace_tset_insert` copies the value before
  `luaH_setint`, then returns the current frame base.  The emitter reloads
  `rbx` from it, so array/hash growth that moves the stack cannot leave the
  trace reading stale slots.
- Existing TSET growth/GC stress tests remain green.
- No new user-facing option; this is a correctness fix.

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

- General allocation safepoints inside traces remain unsupported; GC is
  still deferred to interpreter boundaries where possible.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
