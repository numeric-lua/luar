# Luar 0.57 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release adds
two-integer native C calls and fixes a recorder precheck.

## Highlights

- **`CCALL2II`**: `kill(pid, 0)` executes inside J4 with two integer
  arguments and an integer return, guarded by function identity and both
  argument tags.
- **LOADI/LOADF precheck fix**: the recorder was reading the `sBx` immediate
  as a register number; a zero `LOADI` (encoded as `0x7FFF`) aborted the
  trace before the CALL.  Both full and prefix recorders now skip the
  register check for immediate loads.
- `tests/ffi/32_trace_ccall2ii.lua`; new J4 benchmark.

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

- Two-argument coverage is `(double,double)`, `(double,int)` and
  `(int,int)` with MOVE/LOADK/LOADI arguments.
- `(int,double)`, float32, varargs and aggregate signatures remain CALL exits.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
