# Luar 0.3 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
focuses on making the J4 tracing-IR path measurable and safer, and on the
distribution/ABI surface rather than on broad new trace coverage.

## Highlights

- **J4 benchmark corpus**: `bench/bench.lua j4` (or `FILTER=j4
  ./bench/compare.sh`) covers integer roots, table reads, array→hash side
  traces, one- and two-argument leaf calls, and multi-table bodies.  The
  `j4` group is Luar-only and enables `jit.opt.start("ir=1")`; stock Lua and
  LuaJIT report it as skipped.
- **Two-argument leaf calls** are inlined in J4 roots when the callee is an
  upvalue-free two-parameter function whose body is register `ADD`/`SUB`/
  `MUL` followed by `RETURN1`.  Argument tags and closure identity are
  guarded; unsupported arities and bodies fall back.
- **J4 guard/exit soundness**:
  - table traces no longer suppress guards; table-tag guards use the
    collectable tag, so boolean/float payloads cannot become integer keys;
  - each failing IR instruction reports its own resume PC, the VM
    bounds-checks native exit PCs, and side failures restore the exit
    snapshot and resume at that PC;
  - closure moves execute before `CGUARD`, so a mismatched callee resumes at
    the call with a valid function slot.
- **J4 fallback correctness**:
  - side failures resume the interpreter at the exact exit PC, not the loop
    head, avoiding duplicated prefixes;
  - closure guards report the call PC and the call slot contains the
    function value, so a different callee is handled by the interpreter;
  - root starts are tracked separately from backedges, so interior exits are
    not misclassified as cycles;
  - the raw table-write helper rejects tables with metatables, preserving
    `__newindex`;
  - side-code cache exhaustion no longer leaks generated code;
  - table writes through metatables (`__newindex`) fall back.
- J4 remains **opt-in**.  The new benchmarks show it is correctness-oriented
  and not yet uniformly faster than the default J2/J3 path; no default
  behavior changed.

## Distribution and ABI

The 0.3 distribution work tightens the ABI surface and release path:

- `libluar.so.0.3.0` with SONAME `libluar.so.0`, `libluar.a`, `luar.pc`;
- on macOS, `libluar.0.3.0.dylib` with versioned symlinks and an
  `@rpath` install name;
- the ELF export map now lists only the public `luar.h` functions, not the
  internal J4/JIT helpers; `tests/shared.sh` compares the exact public set;
- installed `luar.pc` is exercised by compiling and running both shared and
  static hosts from a staged prefix;
- `make dist` refuses a dirty tree and `make distclean` removes tarballs and
  scratch output;
- static and shared installs, staged install with `pkg-config`;
- `make dist`/`make test-dist`, install/uninstall;
- FFI on/off, GCC/Clang, JIT=0, ASan/UBSan, deterministic cdef fuzz,
  shared-library and official Lua 5.5.1 suite jobs.

## Verification

At the release commit, the following pass:

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make test-fuzz
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```

The `j4` benchmark group is measured separately with `FILTER=j4
./bench/compare.sh`; benchmark numbers are host-specific and are not a
release gate.

## Known limitations

- J4 table side specializations cover integer array→hash shape changes;
  float table values still fall back.
- Non-inlineable call exits still fall back; J4 does not yet execute call
  continuations natively.
- Chained/multi-level side traces are not implemented; depth is capped and
  tracked, with root-cycle classification.
- Native-ABI exit-stub spilling is replaced by C-level snapshot/restore.
- Packed aggregate calls use the x86-64 thunk matrix; non-x86-64 hosted
  packed callbacks and full macOS ABI verification remain.
- Luar supports only Lua 5.5; there is no 5.1–5.4 compatibility mode.
