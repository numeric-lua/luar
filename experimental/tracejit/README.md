# tracejit: a Lua 5.5 bytecode + DynASM toolchain proof

This directory is a self-contained proof-of-concept for two narrow claims:

1. **Lua 5.5 bytecode can be lifted.**  A C program linked against
   `src/libluar.a` loads a chunk, gets the `Proto` from the loaded
   `LClosure->p`, walks `p->code`, and decodes the numeric-for loop
   (including the `ADD`/`MMBIN` fast path) into a small trace descriptor.
2. **DynASM can emit working x86-64 for that trace.**  The descriptor's
   decoded parameters are fed to DynASM-generated instruction actions at
   run time; the resulting machine code returns the same integer sum as the
   Lua interpreter for all tested `n`.

Nothing here is integrated with the Lua VM.  There are **no guards, no
side exits, no deoptimization, no GC safepoints, no stack maps, no frame
integration, and no code cache**.  See [Scope and limitations](#scope-and-limitations).

## Build and run

From this directory:

```sh
cd experimental/tracejit
make
./demo
```

or, equivalently:

```sh
./run.sh          # runs make, then execs ./demo; nonzero on any mismatch
```

`make clean` removes `demo`, the object files, and the generated
`demo_trace.c`.

The DynASM translation step is exactly:

```sh
lua ../../dynasm/dynasm.lua -D X64 demo.dasc > demo_trace.c
```

`demo.dasc` is a C translation unit with inline DynASM actions.  The
generated `demo_trace.c` is compiled and linked with `demo.c` and
`src/libluar.a`.  `src/libluar.a` is built on demand by the Makefile if it
is missing.

### Host-Lua note

DynASM was written for LuaJIT and originally depended on the Lua 5.1-era
`bit` library.  Luar ships no such compatibility module: the vendored
`dynasm/dasm_ops.lua` implements the 32-bit operations with native Lua 5.5
operators, and the backends require that instead.  The generated
`demo_trace.c` is byte-identical to the pre-port DynASM output, so any stock
`lua` 5.5 (or `../../src/luar`) can run the generator.

## What the demo does

### 1. Load and take the Proto

`demo.c` does:

```c
luaL_loadstring(L, "local s = 0; for i = 1, n do s = s + i end; return s");
```

The loaded closure is an `LClosure`; its `Proto` is obtained with
`clLvalue(...)->p`.  This chunk has no nested function protos
(`p->sizep == 0`), so this is the "inner Proto" referred to by the task.
The program prints every instruction using the opcode names from
`src/lopnames.h`, with operands according to the instruction mode.

### 2. Decode the loop

The bytecode emitted by Lua 5.5.1 is:

```text
pc  0  VARARGPREP
pc  1  LOADI        A=0 sBx=0          s = 0
pc  2  LOADI        A=1 sBx=1          init = 1
pc  3  GETTABUP     A=2 B=0 C=0        limit = _ENV["n"]
pc  4  LOADI        A=3 sBx=1          step = 1
pc  5  FORPREP      A=1 Bx=2
pc  6  ADD          A=0 B=0 C=3        s = s + i
pc  7  MMBIN        A=0 B=3 C=6        __add fallback
pc  8  FORLOOP      A=1 Bx=3
pc  9  RETURN       A=0 B=2 C=1
pc 10  RETURN       A=1 B=1 C=1
```

Two details matter:

* The actual sequence for this chunk is **`ADD` + `MMBIN`**, not
  `ADDI` + `MMBINI`.  Both operands of `s + i` are registers.  The decoder
  also understands the `ADDI` (immediate) form, but this chunk does not
  exercise it.  `MMBIN` is never executed on the integer fast path; a real
  trace JIT would still need a guard/exit for it.
* Lua 5.4/5.5 changed the numeric-for register layout to **three** control
  slots: `R[A]` is the initial value, `R[A+1]` the limit, and `R[A+2]` the
  step before `FORPREP` and the visible loop variable after it.  The loop
  variable here is `R3 = A+2`, not `R[A+3]` as in Lua 5.3.  `FORPREP`
  overwrites the slots with `count`, `step`, and `init`; `FORLOOP`
  decrements the count and updates the control variable.

The decoder checks the jump targets using the VM's post-fetch formulas:
`FORPREP` skips to `pc + Bx + 2`, `FORLOOP` jumps back to `pc - Bx + 1`.
It resolves the accumulator, initial value, step, and limit definitions
from the preceding `LOADI`/`LOADK`/`GETTABUP` instructions and rejects
anything outside the supported shape.

For this chunk the descriptor is:

```text
for_reg = 1, init_reg = 1, limit_reg = 2, step_reg = var_reg = 3
acc_reg = 0, init = 1, step = 1, acc_init = 0
limit_name = "n" (dynamic, comes from _ENV)
op = TRACE_ADD_REG
```

The limit is a runtime global, so it enters the native function as the
SysV argument `n` in `rdi`.  The other loop parameters are decoded
constants and become instruction immediates.

### 3. Emit native code from the descriptor

`demo.dasc` contains an `emit_trace()` function with DynASM actions.  It
uses the decoded descriptor fields directly, for example:

```text
| mov64 rax, ((int64_t)t->acc_init)
| mov64 rcx, ((int64_t)t->init)
| mov64 rdx, ((int64_t)t->step)
|1:
| cmp rcx, rdi
| jg >2
| add rax, rcx
| add rcx, rdx
| jmp <1
|2:
| ret
```

The `jg`/`jl` exit and the accumulator add are selected from the decoded
step sign and opcode kind.  `trace_build()` initializes DynASM, links,
encodes into an anonymous `mmap`, then changes the mapping to read+execute.
No C constants for the loop bounds appear in `demo.dasc`.

The resulting function is `int64_t trace_sum(int64_t n)` in the SysV
x86-64 ABI: `n` in `rdi`, result in `rax`.  It uses only caller-saved
registers and has no stack frame or calls.

### 4. Compare

For each `n` in `{0, 1, 2, 10, 100, 1000, 1000000}` the demo:

1. sets the global `n` (`lua_setglobal`),
2. calls the loaded chunk with `lua_pcall`,
3. calls the DynASM-generated `trace_sum(n)`,
4. compares the two `int64_t` values.

Any mismatch, load error, non-integer Lua result, or code-generation
failure makes the program exit nonzero.  `run.sh` propagates that status.

## Verified output

```text
=== Lua 5.5 chunk ===
local s = 0; for i = 1, n do s = s + i end; return s

=== Proto from LClosure->p ===
  params=0 maxstack=4 code=11 constants=1 nested protos=0
  constants: K[0]="n"

=== Bytecode walk (p->code) ===
  pc  0  VARARGPREP   A=0 B=0 C=0 k=0
  pc  1  LOADI        A=0 sBx=0
  pc  2  LOADI        A=1 sBx=1
  pc  3  GETTABUP     A=2 B=0 C=0 k=0  ; K[0]="n"
  pc  4  LOADI        A=3 sBx=1
  pc  5  FORPREP      A=1 Bx=2  ; prepare R1..R3, skip to pc 9 if no iterations
  pc  6  ADD          A=0 B=0 C=3 k=0  ; R0 := R0 + R3
  pc  7  MMBIN        A=0 B=3 C=6 k=0  ; metamethod fallback (integer fast path skips this)
  pc  8  FORLOOP      A=1 Bx=3  ; if more, R3 += R2 and jump to pc 6
  pc  9  RETURN       A=0 B=2 C=1 k=0
  pc 10  RETURN       A=1 B=1 C=1 k=0

=== Decoded numeric-for trace ===
  FORPREP pc=5 FORLOOP pc=8 (A=1)
  init  R1 = 1   (LOADI @ pc 2)
  limit R2 = _ENV["n"]   (GETTABUP @ pc 3; passed as native rdi)
  step  R3 = 1   (LOADI @ pc 4; R3 is also loop var i)
  acc   R0 = 0   (LOADI @ pc 1)
  body  R0 := R0 + R3   (ADD @ pc 6; MMBIN @ pc 7 skipped by integer fast path)
  native shape: int64_t trace_sum(int64_t n) { int64_t r0 = 0; for (int64_t r3 = 1; r3 <= n; r3 += 1) r0 += r3; return r0; }

=== DynASM x64 native trace ===
  emitted 44 bytes at 0x...
  ABI: int64_t trace_sum(int64_t n), n in rdi, result in rax

=== interpreted (lua_pcall) vs native (DynASM) ===
           n           interpreted                native  result
           0                     0                     0  ok
           1                     1                     1  ok
           2                     3                     3  ok
          10                    55                    55  ok
         100                  5050                  5050  ok
        1000                500500                500500  ok
     1000000          500000500000          500000500000  ok

PASS: 7/7 cases match
```

The emitted size is 44 bytes for the register-add shape; the optional
`ADDI` path emits a different sequence and size.

## Files

| File | Purpose |
| --- | --- |
| `demo.c` | Lua state, `luaL_loadstring`, Proto/bytecode walk, decoder, comparison |
| `demo.dasc` | DynASM x64 source; parameterized emitter and executable-memory builder |
| `demo_trace.c` | generated by DynASM from `demo.dasc` (not checked in) |
| `trace.h` | decoded `TraceDesc` contract shared by both sides |
| `Makefile` | builds `libluar.a` on demand, runs DynASM, compiles and links |
| `run.sh` | builds and runs; exits nonzero on mismatch |

This proof only adds files under `experimental/tracejit/`; it does not
touch `src/`, `vendor/`, the top-level `Makefile`, or other directories,
and it does not commit anything.

## What is proven

* The loaded closure's `Proto` and its instruction stream can be reached
  through the internal C API (`LClosure->p->code`).
* The exact Lua 5.5 numeric-for pattern (`FORPREP`, body `ADD`/`MMBIN`,
  `FORLOOP`) can be recognized and converted into an explicit loop
  descriptor with register mapping and typed operands.
* DynASM can assemble that descriptor at run time into x86-64 code whose
  result matches the interpreter for the tested range, including the
  zero-iteration cases (`n = 0`, and `n` below the initial value).
* The emitted code is parameterized by decoded values: changing the
  decoded initial value, step, accumulator initializer, or immediate
  changes the generated instructions/bytes.  There are no loop-bound
  constants hard-coded in `demo.dasc`.
* The integer addition in the generated loop wraps modulo 2^64 like Lua's
  integer `l_addi`; the tested sums do not overflow.

## Scope and limitations

This is a toolchain proof, not a JIT.

* **Single shape only.**  The decoder accepts exactly one arithmetic body
  op followed by one MM instruction between `FORPREP` and `FORLOOP`.  It
  rejects other bodies, extra control flow, nested loops, etc.
* **No guards or exits.**  The native code assumes the loop is an integer
  loop.  It does not check that `n` is an integer, that the loop values
  did not change, or that a metamethod should fire.  It has no side exits
  and cannot fall back to the interpreter.
* **No deoptimization.**  The demo cannot reconstruct interpreter state or
  resume at a bytecode PC.  There is no OSR entry or exit.
* **No VM/frame integration.**  `trace_sum` is an ordinary C function
  pointer called from C; it is not a Lua function, does not receive a
  `lua_State`, and has no frame or stack map.  The Lua stack is not
  touched by native code.
* **No GC support.**  Native code has no safepoints, roots, allocation
  paths, write barriers, or references to collectable objects.  The demo
  performs no allocation while native code runs.  A real JIT must be able
  to stop at safepoints and describe live references to the collector.
* **No code cache.**  Each build mmaps one tiny page; there is no
  management, patching, invalidation, or W^X transition strategy beyond a
  one-shot `mprotect`.
* **Limited numeric semantics.**  The generated loop uses a simple signed
  `i <= n`/`i >= n` condition.  Lua's integer loop precomputes an unsigned
  iteration count to handle extreme limits and overflow; this POC does not
  reproduce those edge cases.  Float loops, `step == 0` errors,
  metamethods, and integer/float coercions are not implemented.
* **x86-64 Linux SysV only.**  The register assignment, argument register,
  `mmap` flags, and DynASM arch selection are specific to this platform.
  Windows x64, ARM, and macOS are not supported.
## What a real trace JIT still needs

A production trace JIT for this chunk would additionally need:

* an interpreter hot-count/recording hook and trace selection;
* bytecode-to-IR lifting for the full opcode set, not a hand-recognized
  loop;
* **guards** on value types, integer-ness of `n`, loop bounds, and
  assumptions, plus side-exit stubs;
* **deoptimization** and interpreter-state reconstruction at every exit;
* register allocation, spilling, frame layout, stack maps, and unwinding;
* **GC safepoints**, rooting of live values, allocation fast paths, and
  write barriers;
* correct handling of `MMBIN` metamethods, float loops, `step == 0`,
  coercions, and Lua's exact count-based loop semantics;
* trace linking/trees, blacklisting, patching, and code-cache eviction;
* concurrency/thread-safety and W^X strategy for the code cache;
* integration with calls, returns, varargs, upvalues, closures, and
  coroutines.

The demo deliberately stops before all of that so the two toolchain claims
can be inspected in isolation.
