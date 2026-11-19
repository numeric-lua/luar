# LuaR 0.4 release notes

LuaR is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
broadens the opt-in J4 tracing-IR path and fixes the first round of issues
found by benchmarking it.

## Highlights

- **Float table roots**: homogeneous float array cells now execute through
  J4 (`TGETF`, `FMULK`, `FADD`), including float constants such as
  `t[i] * 2.0`.  Integer cells, mixed cell types and float-controlled `for`
  loops fall back to the interpreter.
- **CALL exits**: a loop with a non-inlineable call can execute its supported
  prefix natively; the trace exits at the CALL PC, the interpreter performs
  the call and remainder, and the loop continues.  Counted by
  `jit.stats().ir_call_exits`.
- **New benchmarks**: `j4/float_table` and `j4/call_exit` join the LuaR-only
  J4 benchmark group; the existing `j4/call_leaf`, `j4/call_twoarg`,
  `j4/table_hash_side` and `j4/multi_table` cases remain.
- **Safety fixes**:
  - table-typed arithmetic operands are rejected before installation;
  - float constants retain their double value and are never emitted as an
    integer `imul 0`;
  - table traces keep their tag guards, using the collectable table tag;
  - helper failures preserve an exact exit PC across C helper calls.

## Verification

At the release commit:

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

The GitHub Actions matrix covers GCC/Clang, FFI on/off, JIT=0, ASan/UBSan,
deterministic cdef fuzz, staged install/pkg-config, shared-library FFI and
the official Lua 5.5.1 suite.

## Known limitations

- J4 float support is limited to homogeneous float array cells and
  `ADD`/`MULK` float arithmetic; mixed int/float cells and float-controlled
  loops fall back.
- A Proto has one live root specialization; reusing the same Lua function
  with a different table cell type can force interpreter fallback.
- Chained/multi-level side traces and real parent-depth accounting remain
  future work; the depth cap is recorded and single-level side traces are
  tested.
- Native-ABI exit-stub spilling is replaced by C-level snapshot/restore.
- Packed aggregate calls use the x86-64 thunk matrix; non-x86-64 hosted
  packed callbacks and full macOS shared ABI tests remain.
- LuaR supports only Lua 5.5; there is no 5.1–5.4 compatibility mode.
