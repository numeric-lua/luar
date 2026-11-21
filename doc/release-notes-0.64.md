# Luar 0.64 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
validates the first allocation+write combination in J4 traces.

## Highlights

- With `alloc=1`, a loop that creates `{}`, writes a dynamic integer key and
  reads it back now traces: `TNEW` publishes the table, `TSET` fills it, and
  the existing TGET path reads it.
- GC/stack relocation during table growth is handled by the TSET helper
  returning the relocated base.
- `tests/jit/75_trace_tnew_tset.lua` covers tracing, forced GC and `alloc=0`
  fallback.

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

- Table constructors with `SETLIST`, constant-key `SETI`, closures/strings
  and `ffi.new` remain interpreter-side.
- Allocated tables are not sunk; one table per iteration is retained until
  GC.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
