# ADR 0004 — A loop-level native continuation for J2

Status: accepted for the 0.1 line; J3 is expected to replace it with general
traces.

## Context

The JIT design (`doc/jit-design.md`) targets a tracing compiler, but the
full recorder, SSA IR, exits and register allocator are a multi-month
effort.  LuaR 0.1 nevertheless needs a real performance tier and a truthful
`jit.status()`, not a placeholder.  The numeric benchmarks are dominated by
`for` loops with simple arithmetic bodies, and Lua 5.5 bytecode is already
register-based, so the loop body can run directly on the stack slots.

## Decision

When an integer-controlled `for` loop becomes hot, LuaR emits the
whitelisted body with DynASM and runs the *remaining* iterations natively:

* the loop is entered from `OP_FORLOOP` after the interpreter has executed
  the hot iterations;
* entry guards check every register read initially (integer or double
  flavour) and the loop control slots must be integers;
* the native code implements exact FORLOOP count/index semantics on the
  16-byte `TValue` slots and returns to the VM after the loop;
* type-changing loop-carried registers, unsupported opcodes, aggregate or
  branch operations, and debug hooks fall back to the interpreter;
* the machine code is epoch-tagged; `jit.flush()` releases it, `jit.off()`
  stops using it, and state close frees it.

The whitelist is numeric only: MOVE/LOADI/LOADF/LOADK/ADDI/ADD/SUB/MUL/DIV,
the K variants and MMBIN.  No calls, tables, strings, closures or branches.

## Consequences

* `numeric/int_sum` reaches LuaJIT parity (5.9 ms, 0.17x stock Lua) and
  `numeric/float_loop` is 11.1 ms (0.40x) on the reference machine.
* The engine is small (a few hundred lines of DynASM/emitter) and every
  failure path is an interpreter fallback, so semantics stay with
  `luaV_execute`.
* Coverage is deliberately narrow: `fib`, `mandel` and table/string code are
  unchanged.  The J2 gate's mandelbrot target is not met.
* The implementation hard-codes the default 16-byte `TValue`, 64-bit
  integer and double-double layout and x86-64 SysV; other configurations
  report `jit.status() == false` and interpret.
* J3 must replace this with trace recording/exits; until then the ADR is the
  normative description of what "J2" means in this repository.
