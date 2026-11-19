# J4 tracing-IR foundation

This document describes the first layer of LuaR's tracing subsystem.  It is
deliberately separate from the existing J2/J3 one-pass loop emitter so that
the working JIT can keep shipping while a trace compiler is built.

## Milestone status

The J4 core is implemented: the IR model/verifier, interpreter and guard-exit
recording, exit snapshots/restore, integer/table/K-op/call codegen, interior
side linking with depth/cycle accounting, interpreter safepoints, and live
array-to-hash side traces are all present and tested.  Remaining items are
extensions (native call exits, native-ABI stub spilling, float table
specializations, generalized chained side traces), not prerequisites for the
mid-loop side-trace milestone.

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
- IR-to-x86-64 emission: **implemented for integer and table root traces**
  (constants, moves, guards, arithmetic, K-ops, `TGET`/`TSET`, `FORLOOP`,
  exit/ret), counted by `jit.stats().ir_compiled/ir_code_bytes`.
- Side-trace linker: **implemented for live guard exits**.  Array→hash
  changes link from both native exits and IR-root exits; the IR side path
  executes an interior segment with generic hash lookup, snapshot/restore
  and `FORLOOP` continuation.  Counted by
  `ir_side_traces/ir_side_compiled/ir_side_executed`; `ir_side_start` and
  `ir_root_start` expose the interior start PC; `ir_side_depth` reports the
  linked side depth and `LUAR_IR_MAX_DEPTH` is enforced at link time.
- Leaf calls: **one integer argument and one integer result inlined** for
  upvalue-free leaf bodies (`MULK`/`ADDK`/`SUBK`/`ADDI`), guarded by closure
  identity (`CGUARD`).  Unsupported calls fall back.
- `__index`: table helpers reject tables with a metatable, so the native
  and side paths fall back instead of bypassing metamethods.
- Differential coverage: deterministic array/hash rounds compare IR/side
  results against interpreter-computed expectations.
- Mixed bodies with table reads and inlined leaf calls: **recorded and
  linked**, including interior side exits.
- Multi-table bodies: **recorded and linked**; a hash-shape change in the
  second lookup produces an interior side exit.
- Safepoints: root traces yield to the interpreter every 256 FORLOOP
  iterations (`ir_yields`), so hooks/GC are not starved by a native loop.
- Depth/cycle handling: **implemented and tracked** (`ir_max_depth`,
  `ir_cycles`); depth 1 side segments execute, root-cycle exits are recorded.
- Machine-code emission from the IR: **implemented for integer root and
  interior segments**.
- Table-read IR codegen and helper: **implemented and covered by
  `jit.trace_ir().tget_selftest()`**, and table-read traces execute live
  under `ir=1`.  Table writes (`TSET`) are also emitted and execute live
  when the destination array already has capacity; growth falls back.
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

`LuaR_IRExitState` stores the resume PC, live slot count (up to 64), exact
Lua tag bytes and the 8-byte payload for each slot.  Snapshots are short-lived: they must
not cross a GC safepoint because collectable payloads are not rooted.
Restoring writes stack slots only, so no GC barrier is required.

The J2/J3 emitter reports an exact interior PC in
`jit.stats().last_exit_pc`; the tracing recorder uses the same resume PC and
this snapshot format for side exits.  Live side tests assert that
`ir_side_start > ir_root_start` for prefixed table bodies.

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
side-segment linking is implemented for table guard exits, including
prefixed IR bodies recorded through the `MMBINI` bounds fix.

## Next milestones

1. Extend interior recording to call exits and multi-table bodies.
2. Add exit-stub state spilling to the native ABI (snapshot/restore is in
   place for the IR path).
3. Generalize the side linker beyond array→hash shape changes and enforce
   explicit depth caps for chained side traces.
4. Add differential tests against the interpreter and existing J2/J3 paths
   for all side-exit fixtures.
