# J4 tracing-IR foundation

This document describes the first layer of LuaR's tracing subsystem.  It is
deliberately separate from the existing J2/J3 one-pass loop emitter so that
the working JIT can keep shipping while a trace compiler is built.

## Status

- Trace IR value/guard/exit model: **implemented** (`src/luar_trace.h`).
- Explicit verifier for ops, slots and exits: **implemented**.
- Exit-state snapshot/restore for live stack slots: **implemented**.
- Interpreter-side recorder for simple numeric loops: **implemented**
  (`luar_trace_record_loop`), with counters in `jit.stats()`
  (`ir_traces`, `ir_ops`, `ir_aborts`), including table/K-op loops.
- Interior guard-exit recording: **implemented** for the first exit of a
  Proto, counted by `jit.stats().ir_side_traces/ir_side_ops/ir_side_aborts`.
- Side-link metadata: depth cap 4, cycle detection against the root loop
  target, reported by `ir_max_depth`/`ir_cycles`.
- IR type/liveness analysis: **implemented** for integer and table ops.
- IR-to-x86-64 emission: **implemented for integer root traces**
  (constants, moves, guards, arithmetic, `FORLOOP`, exit/ret), counted by
  `jit.stats().ir_compiled/ir_code_bytes`.  The code is compiled as a
  validation shadow of the existing loop JIT; `jit.trace_ir().exec_selftest()`
  additionally executes a generated trace and checks its result;
  `side_exec_selftest()` compiles/executes a non-loop interior segment.
- Side-trace linker/depth caps/cycle handling: **not implemented**.
- Machine-code emission from the IR: **implemented for integer root and
  interior segments**.
- Live root-trace execution: **implemented behind `jit.opt.start("ir=1")`**;
  legacy JIT compilation is disabled for that loop while IR execution is
  enabled.  `ir_executed` counts successful entries, and unsupported
  traces (float/table ops) fall back to the existing JIT/interpreter;
  `jit.flush()` resets the counter.

`jit.trace_ir().selftest()` exercises IR construction, verification, exit
snapshotting and restore.  `jit.trace_ir().exec_selftest()` compiles and
executes a small integer trace on hosts with native code generation.

## IR model

`LuaR_TraceIR` owns up to 64 instructions and 4 exits.  Instructions carry a
kind, three slot operands, the originating Proto PC and an integer/double
constant.  Supported ops currently cover:

- constants (`KINT`, `KFLOAT`),
- stack moves (`SLOAD`, `SSTORE`),
- tagged guards (`GUARD` with `LUAR_IR_TAG_*`),
- integer/float arithmetic (`ADD`, `SUB`, `MUL`, `DIV`),
- `FORLOOP`,
- exits and return.

The verifier rejects unknown kinds, out-of-range slots, guards with invalid
tags, malformed `FORLOOP` triples, bad exit indices and invalid exit states.

## Exit state

`LuaR_IRExitState` stores the resume PC, live slot count, exact Lua tag bytes
and the 8-byte payload for each slot.  Snapshots are short-lived: they must
not cross a GC safepoint because collectable payloads are not rooted.
Restoring writes stack slots only, so no GC barrier is required.

The existing J2/J3 emitter already reports an exact interior PC in
`jit.stats().last_exit_pc`; the tracing recorder will use the same resume PC
and this snapshot format for side exits.

## Trace-entry state contract

A root trace entry receives the live stack after the interpreter's FORLOOP
has prepared the next body execution.  The trace starts at the loop body,
does not reinitialize count/step/index, and its first action is the recorded
guard set.  `luar_trace_ir_guards_match()` checks those guards against the
live slots; failing guards must fall back to the interpreter before any
mutation.  An interior side segment starts at its recorded resume PC with
the accumulator/control state already advanced by the parent trace.

A first live-entry experiment produced an incorrect loop result because a
DynASM `if/else` scoped the arithmetic store/tag actions incorrectly.  With
braced action blocks, the loop emitter and live root path now produce the
correct result.  The guard contract remains the entry check; interior
side-segment linking is still pending.

## Next milestones

1. Extend interior recording to table/call-heavy exits and compile the
   recorded IR (basic table and K-op recording is already present).
2. Enforce the entry guard contract in the live hook, then add exit-stub
   state spilling to the native ABI.
3. Compile the IR and link side traces with the recorded depth/cycle metadata.
4. Add differential tests against the interpreter and the existing J2/J3
   paths.
