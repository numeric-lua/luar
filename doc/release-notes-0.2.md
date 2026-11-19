# Luar 0.2 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions: a mature opt-in FFI,
a tiered x86-64 JIT, and a new independent J4 tracing-IR subsystem.  The Lua
language, bytecode, C API and module ABI remain those of upstream Lua 5.5.1.

## Highlights

- **J4 tracing IR**, selectable with `jit.opt.start("ir=1")`:
  - typed IR values, guards, exits and a verifier;
  - interpreter hot-loop recording, including table, multi-table, K-op and
    guarded leaf-call bodies;
  - live integer and table-read/write roots;
  - true mid-body side traces for array→hash table shape changes, with
    generic hash lookup, snapshot/restore, depth accounting and `FORLOOP`
    continuation;
  - side exits linked from native guard exits and from IR-root failures;
  - interpreter safepoint yields every 256 iterations so hooks and GC are
    not starved.
- **J2/J3 loop JIT**: guarded numeric `for`/`while`/`repeat` loops,
  int/float accumulators, products, squares, divisions, subtraction,
  branches, table fill/sum intrinsics, partial calls, and simple
  upvalue/leaf/method/`__index` call inlining.
- **FFI 0.2**: cdef macros and local includes, C type model, unions and
  struct arrays by value, bitfields, packed/aligned layouts, variadic
  calls, Lua callbacks with `cb:set`/`cb:free`, TLS globals, cdata
  `string.format` integration, and a real `libluatorch.so` consumer.
- `libluar.so.0.2.0`, SONAME `libluar.so.0`, `luar.pc`, install/uninstall,
  reproducible `make dist`, staged-install CI.
- ASan/UBSan, deterministic cdef fuzzing, differential, shared-library,
  CLI/ABI and official Lua 5.5.1 suite coverage.

## J4 details

J4 is deliberately independent of the J2/J3 one-pass loop emitter.  The IR
model (`src/luar_trace.[ch]`) records up to 64 instructions and four exits,
verifies slot/guard/exit consistency, and compiles through the same DynASM
assembler.  Exit snapshots cover up to 64 stack slots; restores write stack
slots only.  Side traces are linked with a depth cap (4), root-cycle
classification and per-Proto code storage released by `jit.flush()`.

The live IR path is opt-in.  With `ir=0` (default), existing J2/J3 behaviour
is unchanged.  Unsupported shapes — float table specializations, table
growth, non-inlineable call exits and metatable reads — fall back to the
interpreter or the existing JIT rather than miscompile.

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
```

The GitHub Actions matrix covers GCC/Clang, FFI on/off, JIT=0, ASan/UBSan,
deterministic cdef fuzzing, staged install with pkg-config, shared-library
FFI and the official Lua 5.5.1 suite (31/31 files plus canonical `final OK`).

## Known limitations

- Generic table JIT tiers remain opt-in and are not consistently faster than
  the interpreter on the reference corpus.
- J4 handles integer leaf calls; non-inlineable call exits still fall back.
- J4 table side specializations currently cover array→hash shape changes and
  integer values; float table values fall back.
- Native-ABI exit-stub spilling is replaced by C-level snapshot/restore.
- Packed by-value aggregate calls use the x86-64 thunk matrix; non-x86-64
  hosted packed callbacks and full macOS ABI verification remain.
- Luar supports only Lua 5.5; there is no 5.1–5.4 compatibility mode.
