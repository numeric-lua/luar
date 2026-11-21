# Luar 0.61 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
makes `jit.flush()` safe while native code is executing.

## Highlights

- **Deferred flush**: the JIT state tracks active native-trace depth.
  `jit.flush()` seen during a trace bumps the epoch and marks a pending
  free; the outermost trace return performs the actual `free_code()` after
  generated code is off the C stack.
- Finalizers that call `jit.flush()` during traced TSET growth no longer
  risk unmapping the executing trace.
- `tests/jit/73_flush_deferred.lua`.

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

- General trace allocation is not implemented; the TSET-growth path remains
  the only allocation-capable helper and it is GC-rooted.
- The first `TNEW` opcode still requires the non-allocating TSET split and
  the allocation test matrix in `doc/allocation-gc-traces.md`.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
