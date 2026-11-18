# J4 tracing-IR foundation

This document describes the first layer of LuaR's tracing subsystem.  It is
deliberately separate from the existing J2/J3 one-pass loop emitter so that
the working JIT can keep shipping while a trace compiler is built.

## Status

- Trace IR value/guard/exit model: **implemented** (`src/luar_trace.h`).
- Explicit verifier for ops, slots and exits: **implemented**.
- Exit-state snapshot/restore for live stack slots: **implemented**.
- Interpreter-side recorder: **not implemented**.
- Side-trace linker/depth caps/cycle handling: **not implemented**.
- Machine-code emission from the IR: **not implemented**.

`jit.trace_ir().selftest()` exercises IR construction, verification, exit
snapshotting and restore.

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

## Next milestones

1. Record IR from the interpreter at a hot loop and from an interior guard
   exit.
2. Add exit-stub state spilling to the native ABI.
3. Compile the IR and link side traces with a depth cap.
4. Add differential tests against the interpreter and the existing J2/J3
   paths.
