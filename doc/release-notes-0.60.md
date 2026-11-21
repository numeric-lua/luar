# Luar 0.60 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
closes a native C-call callback escape.

## Highlights

- The zero-argument `CCALL0I` classifier and helper now reject
  `ffi.callback` cdata, owned/ref cdata and ctype handles, matching the
  stricter one- and two-argument forms.
- A callback invoked in a traced loop remains interpreter-side; a CALL-exit
  prefix may still exist, but generated code never enters the callback.
- `tests/ffi/34_trace_callback_reject.lua` covers the callback path and
  confirms a normal zero-argument C function still traces.
- **Stack-relocation safety**: the J4 entry wrapper now saves/restores
  `L->top` as a stack offset, and the interpreter reloads `base` after every
  JIT hook and continuation. This keeps execution correct if a helper's GC
  step moves/reallocs the Lua stack.

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

- `jit.flush()` during an active native trace is still not deferred.
- Trace allocation remains limited to the GC-rooted TSET-growth path; there
  is no general allocation opcode.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
