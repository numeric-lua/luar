# Luar 0.39 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
verifies continuation safety across coroutine suspensions.

## Highlights

- **Yield/resume continuations**: a traced prefix exits before a yielding
  call; the interpreter yields, and on resume the suffix continuation executes
  and the loop continues.
- `tests/jit/70_call_yield.lua` resumes 100 yields and checks the final sum,
  resume count and `ir_cont_runs`.
- Erroring calls continue to unwind without running the suffix.

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

- `B == 0` calls remain unsupported in prefixes.
- Arbitrary pointer arithmetic falls back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
