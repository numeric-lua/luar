# Luar 0.25 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
native continuation of the suffix after a traced CALL.

## Highlights

- **Post-CALL continuation**: when a partial trace exits before a CALL, the
  suffix from `callpc + 1` to the backedge is compiled separately.  After the
  interpreter executes the call, the suffix runs natively, then the
  interpreter executes `FORLOOP` and re-enters the prefix.
- Works for C functions, callable tables and Lua callees; both interpreter
  return paths call the continuation hook.
- `ir_cont_compiled` and `ir_cont_runs` report compilation/execution.
- `tests/jit/69_call_cont.lua` covers C and Lua callees.

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

- The suffix must not depend on the call result; such dependencies still fall
  back.
- `B == 0` calls and error/yield paths in continuations are not specialized.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
