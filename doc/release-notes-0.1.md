# LuaR 0.1 release notes

LuaR is a Lua 5.5.1 runtime with LuaJIT-style extensions: an opt-in FFI and a
loop-level x86-64 JIT.  The language, bytecode, C API and module ABI are
those of upstream Lua 5.5.1.

## Highlights

- `luar`, `luarc`, static/shared `libluar`, pkg-config, install/uninstall
  and reproducible `make dist`.
- Exact 5.5 semantics; no 5.1–5.4 compatibility modes, `global` reserved.
- `table.new` / `table.clear`; truthful `jit.status()`.
- FFI behind `WITH_FFI=1`: cdef macros/includes, cdata, struct/union
  bitfields, packed/aligned layouts, varargs, callbacks, TLS, packed
  integer calls/returns and packed-argument callbacks on x86-64.
- JIT: J1 recorder/dumps/stats; J2 guarded numeric `for`/`while`/`repeat`
  loops with integer/float sums, divisions, products, squares, subtraction
  and linear accumulators.  Raw table fill/sum intrinsics are on by default;
  generic table emission and partial calls are opt-in.
- ASan/UBSan, differential, fuzz, shared-library, install, official Lua
  suite and real `libluatorch.so` coverage.

## Known limitations

- Packed by-value FFI is implemented through x86-64 ABI thunks; other hosts
  reject unsupported packed call shapes.
- Table JIT tiers are opt-in and not yet faster than the interpreter on the
  reference corpus.
- Full Lua call inlining, side traces and J4 tracing are not implemented;
  `jit.stats().exits` counts guard failures as the seed for that work.
- Full macOS ABI verification remains; CI builds the shared library there.

## Verification

`make test`, `make WITH_FFI=1 test`, `make JIT=0 test`, `make test-asan`,
`make test-fuzz`, `make shared test-shared`, `tests/upstream/run.sh` and
`make test-consumer` pass at the release commit.
