# Luar 0.22 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
widens CALL exits beyond inlineable Lua closures.

## Highlights

- **C-function CALL exits**: a loop prefix can run natively up to a call to a
  C function such as `math.abs`; the interpreter performs the call and
  resumes, with the loop prefix still traced on the next iteration.
- **Callable-table CALL exits**: tables with `__call` follow the same
  exit-at-CALL contract.
- Inline leaf-call specialization still takes precedence when its
  preconditions hold.
- `tests/jit/68_call_c_exit.lua` covers both shapes and checks
  `ir_call_exits`.

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

GitHub Actions also runs the macOS shared/install/module job.

## Known limitations

- CALL exits still hand the call itself to the interpreter; there is no
  native continuation of the suffix after the call.
- `B == 0` calls remain unsupported in prefixes.
- Native exit-stub spilling remains C-level snapshot/restore.
- Nested aggregate/pointer field access stays interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
