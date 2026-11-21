# Luar 0.59 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes GC rooting for allocation-capable J4 helper paths.

## Highlights

- **Rooted J4 entry**: every native trace entry sets `L->top` to cover the
  whole Lua frame while the trace runs, and restores it on return.  Helpers
  such as TSET hash insertion can now allocate/trigger GC without leaving
  live frame slots outside the GC root set.
- **TSET GC stress**: `tests/jit/72_trace_tset_gc.lua` inserts 120k keys
  through a traced loop and validates the table afterward.
- No behavior change for non-allocating traces.

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

- Trace allocation is still not a general feature; this only makes the
  existing TSET-growth path GC-safe.
- `jit.flush()` while a native trace is active is still not deferred.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
