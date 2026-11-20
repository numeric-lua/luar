# Luar 0.54 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
connects cdata double reads to native C-call arguments.

## Highlights

- **`fabs(d[i])` traces natively**: the immediately preceding CGETF, CPGETF
  or scalar float CGETFIELD writes the CALL argument slot; a runtime float
  guard validates it before `CCALL1DD`.
- Lua-table/int arguments on the same Proto fall back without reaching the
  double call op.
- Direct cdata double reads remain unchanged.
- `tests/ffi/28_trace_ccall1dd_cgetf.lua` covers array and pointer-field
  arguments.

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

- Only one double argument with a direct preceding cdata read is supported.
- Multiple arguments, varargs, float32 and aggregates remain CALL exits.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
