# Luar JIT design (tracing, memory-based, x86-64 first)

Status: design for milestones 0.4–0.7 / J1–J5, with a partial implementation
of J1/J2 (see the status section below).  Normative for the JIT work.
`luaV_execute` remains the semantic authority; a trace may run only when it
is provably observationally equivalent to that interpreter.

## Implementation status (0.1)

- **J1 implemented.**  Hot backedges are counted per `Proto`; the bytecode
  of the loop is snapshotted into a trace; `jit.stats`/`trace`/`traces`/`ir`,
  `jit.dump("t"/"i")`, `jit.flush`, `jit.off`, `jit.opt.start("hotloop=N")`
  for newly created Protos and the epoch-based invalidation exist.
- **J2 partially implemented (x86-64 only).**  This is a loop-level native
  continuation rather than a general trace: when an integer- or
  float-controlled `for` loop becomes hot, the whitelisted body (numeric
  MOVE/LOADI/LOADF/LOADK/ADDI/ADD/SUB/MUL/DIV, the K variants, integer
  floor division/modulo, integer bitwise, variable and constant shifts,
  unary minus/not, numeric `TEST`/`TESTSET`, `MMBIN`, numeric comparisons
  and forward `JMP` branches) is emitted with DynASM for `for`, simple
  `while` and `repeat ... until` loops.  A leading constant-index array
  read (`GETI`, e.g. `t[1]`) is also compiled with the split-array bounds
  and tag guards.  Entry tag guards check every register read initially; the native
  code runs the remaining iterations on the Lua stack slots and returns to
  the VM after the loop.  Mixed int/double operands are handled with the
  conversions Lua performs, and both integer- and float-controlled `for`
  loops (including bodies with branches) use the interpreter's
  limit/step/index layout with NaN-aware termination and hand the next
  iteration to the compiled code.  The default 16-byte
  TValue, 64-bit integer and double layout is checked before compiling, as
  is the 5.5 split array representation used by raw table access.
- **Deviations from this design.**  There is no general trace recorder, no
  exit stub/side-exit chain, no safepoint or GC barrier machinery (the
  compiled loop cannot allocate, call, or raise), no general
  table/string/closure operations; comparisons/forward branches and numeric
  truth tests are compiled for int and double operands; an early J3 experiment (raw array and keyed-hash fill/sum intrinsics are on
  by default; generic
  table emission is behind `jit.opt.start("table=1")`)
  compiles a leading integer array lookup/store, the accumulating ADD and
  register-cached fill/sum loops.
  Conditional backedges from `repeat ... until` are recorded and compiled,
  with register-cached sum/subtraction/product/square fast paths (integer
  or float); other backward inner loops still fall back.  The raw-array
  tier also has a `s = s + a[i] + b[i] + c[i]` intrinsic over three arrays.
  A loop whose carried registers change type between iterations is rejected
  (the emitter guards tags only at entry), including straight-line bodies
  that used to be checked only when they contained a branch.  A `Proto` keeps the first compiled
  loop and the most recently compiled one (two slots); a 64-bit bloom of
  hot backedges prevents retrying rejected loops.  Guard failures increment `jit.stats().exits` and a per-code-slot counter;
  a failed guarded loop can compile a second numeric variant from the live
  frame (counted by `jit.stats().side_traces`) once `hotexit` (default 10, `jit.opt.start("hotexit=N")`) exits have
  accumulated (for example an
  int loop followed by a float loop) before returning to the interpreter.
  Full side traces remain future work.  The code cache is capped
  at 64 mappings and released by `jit.flush`/state close.
- **Gate status.**  `numeric/int_sum` is ~9x faster than the interpreter
  (ahead of LuaJIT), `numeric/float_loop` ~4x, and `numeric/mandel` is now
  ~1.5x (13.5 ms versus 20.9 ms on the development host) because a rejected
  outer loop no longer prevents the inner `while` from compiling.  The J2
  gate still asks for ≥3x on `mandelbrot`; `fib` remains call-bound
  (~1.0x).  `jit.flush()` now resets per-Proto hot state through the epoch,
  so flushed loops become hot and recompile instead of staying disabled.

## 1. Goals, non-goals and the correctness contract

**Goals:** a tracing JIT for Lua 5.5.1 bytecode with ≥3× on call-free numeric loops and `mandelbrot` in `bench/` (recursive `fib` is call-bound; see section 12); 1.5–3× on table/object code; FFI kernels within
30% of LuaJIT 2.1; exact semantics; interpreter fallback for every guard, abort, blacklist and `jit.off()`; truthful `jit.on/off/flush/status/opt`; x86-64 SysV
first, ARM64/macOS next. **Non-goals:** legacy Lua semantics, LuaJIT bytecode/ABI, NaN-boxing, AOT/whole-program compilation, JIT for C modules, replacing the
interpreter, or promising LuaJIT parity; `jit.util`/`jit.profile`/`jit.attach` stay out of scope.

**Contract.** With traces on and with `jit.off()`, every program must produce identical (1) values and language-visible side effects; (2) order, arguments and
results of metamethod calls; (3) errors, `pcall`/`xpcall`, error objects, messages, levels and `chunk:line:` prefixes; (4) coroutine yield/resume and `<close>`
finalisation order; (5) reachability/finalisation at `lua_close`; (6) debug-hook event sequences (traces are disabled while hooks are active); (7) integer
results under two's-complement wrap and floats under IEEE double with the host libm. Exact allocation counts, GC step timing and `collectgarbage("count")` are
excluded; no compiled program may observe a different live object, finalizer order or error.

Operational rule: **a trace performs only side effects it can complete with the interpreter's exact semantics.** Arithmetic only for two numbers; table access
only when the raw slot is present (so no `__index`/`__newindex`); comparison only for numbers/strings/booleans (so no `__lt`/`__le`/`__eq`). Everything else
exits at the bytecode PC *before* the effect. A trace never constructs an error, calls a metamethod, or performs a speculative store the interpreter would not.
No fast-math, reassociation, FMA contraction or aliasing tricks.

## 2. Why tracing, and the value-model constraint

Lua 5.5 bytecode is a 32-bit register format (`Instruction`, 4 bytes; max 255 registers; `src/lopcodes.h:206-215,231-348`). It is fully dynamic: a slot can be
an integer on one iteration and a table on the next, and `OP_MMBIN*` exists because arithmetic must fall back to metamethods (`src/lopcodes.h:355-412`). There
is no type feedback in the bytecode. A method JIT would compile every `Proto` generically; tracing records the actual hot path, specialises on observed tags and
leaves the rest to the interpreter. AOT is not viable: chunks arrive through `load`, `string.dump`, C modules and `require`.

`TValue` is 16 bytes (`src/lobject.h:65-69`; verified `sizeof(TValue)==16`, `offsetof(TValue,tt_)==8`), an 8-byte `Value` union plus a tag byte; integers are
`int64_t`, floats `double`; tags distinguish `LUA_VNUMINT` (3), `LUA_VNUMFLT` (19), table (5), short string (4), long string (20), etc. Stack slots are
`StackValue` (16 bytes; `delta` is only TBC bookkeeping, `src/lobject.h:140-158`). We keep this and do **not** adopt LuaJIT's 8-byte NaN-boxed `TValue`
by LuaJIT's published design notes and source, not copied from its legacy value model.

The design is a **memory-based tracing JIT**: (1) `base = ci->func.p + 1`; register `A` is the canonical `TValue` at `base+A` (`src/lvm.c:1108-1116`). (2)
Inside a trace, values are unboxed into registers: `int64` in GPRs, `double` in XMM, pointers/tags as needed. (3) Before any call, allocation,
metamethod-capable operation, GC point or exit, values spill back to canonical slots. (4) No stack pointer is cached across a safepoint: reload `base` from
`ci->func.p+1`, because `luaD_reallocstack` moves the stack and corrects every pointer (`src/ldo.c:330-354`; it also sets `ci->u.l.trap`). (5) A trace owns no
`CallInfo`; it is a subroutine of the current frame, returns a PC on exit and the interpreter continues there. There is no compiled-frame deoptimisation.

This costs load/store traffic versus LuaJIT's register-allocated traces. An SSA pass promotes slot loads/stores to registers and inserts spills at safepoints
(section 4). The payoff: errors, GC, hooks, open upvalues pointing into the stack, stack reallocation and `jit.flush()` all see a normal interpreter frame.

## 3. Opcode map

**I** = inline; **H** = VM helper (spill + safepoint); **X** = interpreter bailout. Guards are the conditions for **I**; failure exits at the opcode PC unless
stated. J1 records only; phase tags say when a row becomes real.

| Opcode(s) | Mode | Guards / notes |
| --- | --- | --- |
| `MOVE` | I | None. 16-byte tag+payload copy; alias a tracked unboxed value. Collectable copy needs no barrier (stack slot is a root). |
| `LOADI`, `LOADF`, `LOADK`, `LOADKX` | I | None; `LOADK`/`LOADKX` have known `Proto->k` type; `LOADKX` consumes `EXTRAARG` (`src/lvm.c:1262-1268`). |
| `LOADFALSE`, `LFALSESKIP`, `LOADTRUE`, `LOADNIL` | I | None; `LFALSESKIP` skips the next (`LOADTRUE`); `LOADNIL` writes `B+1` slots (`src/lvm.c:1269-1292`). |
| `GETUPVAL`, `SETUPVAL` | I (+H barrier) | Entry guard `cl` is `LClosure`; load/store `cl->upvals[B]->v.p` then tag-guard. Open upvalue aliases a stack slot: reload after safepoints, invalidate cached slots. `SETUPVAL` emits `luaC_barrier`; call `luaC_barrier_` only on black/white collectable (`src/lgc.h:245-249`). |
| `GETTABUP`, `GETTABLE`, `GETI`, `GETFIELD` | I / X | Integer key: guard key tag, `0<idx<=asize`, array tag non-empty (`luaH_fastgeti`, `src/ltable.h:51-55`). Short string: hash lookup with node-key/value guards. Float-integral/other keys, miss, or non-table: X (`__index`). `GETI` guards `1 <= C <= asize` (i.e. `(unsigned)(C-1) < asize`); `GETFIELD` guards the node (`src/lvm.c:1306-1354`). |
| `SETTABUP`, `SETTABLE`, `SETI`, `SETFIELD` | I + H barrier / X | Fast path only when the raw slot is present and non-nil (no `__newindex`), or array slot in range and `checknoTM(mt,TM_NEWINDEX)` (`src/ltable.h:57-66`). Guard table/shape/key/tag; store value+tag, then `luaC_barrierback` on black/white (`src/lgc.h:251-255`). New key, resize, nil slot with metamethod, wrong type: X. |
| `NEWTABLE` | H | `luaH_new` + `luaH_resize`, with `L->top = ra+1` as the interpreter (`src/lvm.c:1413-1433`). Full safepoint; decode k/extra arg. |
| `SELF` | I / X | Copy receiver to `A+1`, then `GETFIELD` fast path. Miss: X at `SELF` PC (`__index`; rewriting `A+1` is idempotent). |
| `ADDI`, `ADDK`, `SUBK`, `MULK` | I | Guard operand integer, or float for a float constant; wrap for ints. Other tags: X (MMBIN). `ADDI` uses `sC` (`src/lvm.c:1446-1461`). |
| `MODK`, `IDIVK`, `MOD`, `IDIV` | H (J2), I later | Guard divisor nonzero; integer `-1` via `luaV_idiv`/`luaV_mod`, float via `luaV_modf` (`src/lvm.c:772-822`). Inline only after zero/`-1` are proven. |
| `POWK`, `DIVK`, `POW`, `DIV` | I / H | `DIV`/`DIVK`: both numbers, `divsd`. `POW`/`POWK`: both numbers, call `pow` (libm, no fast-math). Non-numbers: X. |
| `BANDK`, `BORK`, `BXORK`, `BAND`, `BOR`, `BXOR` | I / X | Both integer or float with exact integer representation (`tointegerns`, `src/lvm.h:56-70`); int64 bitwise. Otherwise X. |
| `SHLI`, `SHRI`, `SHL`, `SHR` | I / X | Integer-convertible operands and shift `0..63`; outside that range X (`luaV_shiftl`, `src/lvm.c:824-838`). `SHR` is logical, not arithmetic. |
| `ADD`, `SUB`, `MUL` | I | Guard both integers (wrap `add`/`sub`/`imul`, low 64 bits) or both floats (`addsd`/`subsd`/`mulsd`). Mixed/non-number: X at the arithmetic PC; interpreter handles `MMBIN`. |
| `MMBIN`, `MMBINI`, `MMBINK` | X | Never compiled; executed only when the preceding arithmetic failed. Recorder aborts if reached (`src/lvm.c:1562-1591`); the trace guards the fast path instead. |
| `UNM`, `BNOT` | I / X | Integer `0-x` wrap or float sign flip; `BNOT` requires integer-convertible. Otherwise X. |
| `NOT`, `TEST`, `TESTSET` | I | None; truthiness is tag-based for any value. `TESTSET` then copies and follows the recorded branch. |
| `LEN` | I (string) / H or X (table) | String: tag guard, load `shrlen`/`lnglen` (`src/lvm.c:737-754`). Table: only if no `TM_LEN`; `luaH_getn` helper or X. Other: X. |
| `CONCAT` | H / X (J4) | `luaV_concat` (`src/lvm.c:690`) with `top=ra+n`, `savedpc=pc+1`; may allocate/call `__concat`/yield. J1–J3: X. |
| `CLOSE`, `TBC` | X | Closing can yield/error/reallocate (`luaF_close`/`luaF_newtbcupval`, `src/lfunc.c:172-260`). Never inside a trace. |
| `JMP` | I | None; follow the recorded direction. Backward `JMP` is a loop backedge (section 7). |
| `EQ`, `EQK` | I / X | `EQ`: numbers (exact int/float), short strings (pointer), booleans/nil (tag), collectables (identity); guard tags. `EQK` is raw equality (`luaV_rawequalobj`), no `__eq`. Tables/userdata with different pointers, long strings, mixed variants: X. |
| `LT`, `LE` | I / X | Guard both numbers or both strings. Numbers use Lua's mixed int/float rules (`LTnum`/`LEnum`, `src/lvm.c:400-577`); strings use `strcoll`, not `memcmp`. Other: X (metamethod). |
| `EQI`, `LTI`, `LEI`, `GTI`, `GEI` | I / X | Guard operand numeric; immediate is a small integer. Mixed int/float follows `LTintfloat` semantics or exits; `EQI` with a non-number is false, not an error. Other tags for order: X. |
| `CALL`, `TAILCALL` | X (J1–J3), H later | Calls can yield, move the stack, run hooks and call arbitrary code. J4 may call `luaD_call` for a known C function with a full safepoint, but a yield kills the trace; Lua-to-Lua inlining is beyond J5 (`src/ldo.c:671-787`). |
| `RETURN`, `RETURN0`, `RETURN1` | X | Interpreter performs `luaD_poscall`, closes upvalues when `k`, handles varargs (`src/lvm.c:1769-1836`). Trace exits before it; `RETURN1` is the common loop exit. |
| `FORLOOP`, `FORPREP` | I / X preheader | `FORPREP` runs once before the loop; trace starts at the header (`src/lvm.c:1855-1861`) and guards post-`FORPREP` slot types. `FORLOOP`: guard `R[A..A+2]` integers (the interpreter gates on the step at `R[A+1]`); use unsigned `count != 0` (Lua stores `l_castS2U(count)`; signed compare is wrong at the high bit). Decrement count, add step to index with wrap. Float: guard all three float, `floatforloop` semantics with NaN-safe branches. No overflow guard. |
| `TFORPREP`, `TFORCALL`, `TFORLOOP` | X / H / I | `TFORPREP` swaps control/closing and creates a TBC upvalue (`src/lvm.c:1862-1880`): X. `TFORCALL` calls an arbitrary iterator via `luaD_call`, may yield: X (J1–J3), H J4. `TFORLOOP` is just a nil test and branch: I. |
| `SETLIST` | I / X | Only fixed `B`, table tag, `last<=asize`, no resize; store tag+value and `luaC_barrierback` per collectable (`src/lvm.c:1907-1933`). `B==0` (top), extra arg `k`, resize: X. |
| `CLOSURE` | H / X | `pushclosure` (static in `lvm.c`; call `luaF_newLclosure` + `luaF_findupval` + barrier, or expose a tagged wrapper) allocates, finds/creates upvalues, can GC (`src/lvm.c:840-859,1935-1941`). J1–J3: X; J4 helper with full safepoint. |
| `VARARG`, `GETVARG`, `VARARGPREP` | X | Layout depends on `L->top`/`ci->u.l.nextraargs`; `VARARGPREP` changes `base` (`src/lvm.c:1942-1969`). Never recorded; loops after vararg setup can still trace. |
| `ERRNNIL`, `EXTRAARG` | X / decoder | `ERRNNIL` is an error path with a global-name constant (`src/lvm.c:1955-1960`, `src/ldebug.c:817-823`). `EXTRAARG` is never executed (`src/lvm.c:1970-1973`); consumed by `LOADKX`/`NEWTABLE`/`SETLIST`. |

Cross-cutting: every arithmetic **I** assumes the recorder saw success and therefore never saw its `MMBIN`; the recorder checks the next PC and aborts if it is
the corresponding `MMBIN`. A failing tag guard exits at the **opcode PC**; a failing **branch** guard on a pure test may exit directly at the other successor
PC. Keep the two exit kinds separate. Immediates (`sBx`, `sC`, `sJ`, `vB`, `vC`, `k`, `Ax`) are decoded per `src/lopcodes.h:100-200`.

## 4. IR design

**Form.** Linear SSA over one trace, with labels, explicit branches and `PHI` only at the loop header. A trace is a single-entry, multiple-exit region with one
root loop header; inner backedges only from J4. Instructions are side-effect-ordered against memory operations.

**Kinds.** `i64` (Lua integer/raw bits; wrap), `f64` (IEEE double, no excess precision), `ptr` (non-collectable pointer; not a GC root), `slot` (`TValue* base+16*A`, a derived address, not a Lua value), `tab` (`Table*`, always shape-guarded), `str` (`TString*`), `nil`/`bool`, `tag` (8/32-bit tag or small count).
Collectable pointers always come from a slot or `Proto->k`, never invented.

**Ops.** Constants (`KINT`, `KFLOAT`, `KPTR`, `KNIL`, `KBOOL`, `KSLOT`); stack (`SLOAD`, `SSTORE`, tags); upvalues (`UGET`, `USET`, `UBARRIER`); tables (`TGET`,
`TSET`, `TGETI`, `TSETI`, `TNEW`, `TLEN`, `TBARRIER`); arithmetic (`ADD`/`SUB`/`MUL` wrapping i64, `DIV`, `POW`, `MOD`, `IDIV`, bitwise, `UNM`/`BNOT`);
conversions (`I2D`, `D2I_EXACT`, `TAG`, `PAYLOAD`); compare (`LT`, `LE`, `EQ`, `NE`, `ULT` for loop counters); control (`GUARD` with exit id, `JUMP`, `BRANCH`,
`PHI`, `LOOP`, `EXIT`, `SAFEPOINT`); calls/allocation (`CALL_HELPER` with fixed C signature, `ALLOC`, `BARRIER`); debug (`DUMP`).

**Memory model.** Loads/stores are ordered. No alias analysis in J1: any `TSET`, `USET`, `CALL_HELPER`, `ALLOC`, `BARRIER` or `SAFEPOINT` invalidates cached
table/upvalue facts and possibly-aliasing slot loads. A stack store invalidates that slot; an upvalue store invalidates all stack slots because an open `UpVal`
can point anywhere in the frame. After such an op the IR reloads and re-guards. Traces are short; re-recording is cheaper than a wrong hoist.

**Metadata.** A `LuaR_Trace` holds `next`, `Proto *proto` (rooted; keeps constants alive), `Instruction *startpc`, `entry_sig` (tags of live entry slots), IR
array, code pointer/size, exit array, `maxslot`, hit counters. A `LuaR_Exit` holds `pc` (instruction index), the interpreter `ntop`, a spill-map index, hit count and optional
side-trace pointer. Spill maps are validated by `LUAR_JIT_DEBUG`.

**Worked example.** For `local s=0; for i=1,n do s=s+i end; return s`, `luarc -l` (function form) gives:

```text
1 LOADI 1 0      ; s=0
2 LOADI 2 1      ; count=1
3 MOVE  3 0      ; limit=n
4 LOADI 4 1      ; step=1
5 FORPREP 2 2    ; ra=2: count, step, index
6 ADD   1 1 4    ; s=s+i  <- loop header
7 MMBIN 1 4 6    ; skipped on success
8 FORLOOP 2 3    ; to 6
9 RETURN1 1
```

The hot backedge is PC 8; the trace starts at PC 6 with `R1..R4` integers. `FORLOOP` reads the **old** count; if `count>0` (unsigned) it writes `count-1`, adds
step to index and jumps back. The body runs once per entry, and the old count decides whether to loop again:

```text
GUARD_TAG R1,INT -> E_ENTRY(pc=6)   ; same for R2, R3, R4
L_header:  s0=PHI(s_entry,s1); cnt0=PHI(cnt_entry,cnt1); idx0=PHI(idx_entry,idx1)
  s1   = ADD.wrap s0, idx0          ; ADD 1 1 4
  GUARD cnt0 != 0 -> E_END          ; unsigned; FORLOOP tests old count
  cnt1 = SUB.wrap cnt0, 1
  idx1 = ADD.wrap idx0, 1           ; step constant from LOADI 4
  JUMP L_header
E_END:                              ; pc=9, RETURN1
  SSTORE R1, INT(s1)
  SSTORE R2, INT(cnt0)              ; FORLOOP did not change count
  SSTORE R4, INT(idx0)              ; ... nor index
  EXIT pc=9, ntop=0
```

Each code slot stores its entry guard signature; `try_run` dispatches to
the matching variant first and reports hits via `jit.stats().sig_hits`.
The tracing-IR foundation (`src/luar_trace.c`, `doc/j4-tracing-ir.md`) now
defines IR ops, guards, exits and live-slot snapshots, with a verifier and
selftest.  A numeric-loop recorder is wired into the hot hook and reports
`jit.stats().ir_traces/ir_ops/ir_aborts`; interior-exit recording and the
side-trace linker are still to come.
Mid-loop table guards now return the exact failing instruction PC to the
interpreter and record it in `jit.stats().last_exit_pc`; entry guards resume
at the loop target.  This is the state-capture half of J4 side exits; no
side-trace recording/linking is implemented yet.

Guards: (1) entry tags `R1..R4` integer; if `n` is float, `FORPREP` made a float loop, so record a separate signature or fall back. (2) loop condition `cnt0 != 0` unsigned, the only data-dependent guard; if it fails, the body has already run and `s1` is the result, while count/index are unchanged, matching `FORLOOP`.
(3) no overflow guard: `ADD`/`SUB` wrap, as `intop` (`src/lvm.c:933-938`) does. (4) no metamethod guard: both operands are ints, the following `MMBIN` is
unreachable, and the recorder verifies PC 7 is skipped. On an entry tag failure the trace exits at PC 6; the interpreter runs `ADD`, may execute `MMBIN` at 7,
then `FORLOOP`.

## 5. Guards, exits and deoptimisation

Each exit records `(pc, ntop, spill-map)`. On failure the stub (1) writes every live unboxed value to its canonical slot in spill-map order (full `TValue`:
payload then tag byte); (2) sets `L->top = base + ntop`, recorded from the interpreter at that exit; (3) returns the exit id; (4) the C hook sets `ci->u.l.savedpc = p->code + pc` (the instruction to
execute), calls `updatebase(ci)`/`updatetrap(ci)`, and lets `luaV_execute` fetch that instruction. There is no register-state reconstruction: after the spill
the interpreter sees an ordinary frame. A trace owns no `CallInfo`, so there is no compiled frame to unwind and no bailout table beyond `(pc, ntop, spill)`.

A helper that emulates bytecode `p` and can raise/yield sets `savedpc = p->code + p + 1`, because `pcRel(pc,p) = (pc - p->code) - 1` (`src/ldebug.h:14`) makes
the error line `p`. An exit that re-executes `p` uses `savedpc = p->code + p`. An off-by-one here changes error line numbers, which the differential suite
checks.

| Hazard | Guard | On failure |
| --- | --- | --- |
| Subtype change | tag byte of each loaded slot | exit at reading opcode; interpreter re-dispatches, including `MMBIN` |
| Integer overflow | none; wrap is semantic | — |
| Divide by zero / `INT_MIN // -1` | `divisor != 0`, `!= -1` before inline `//`/`%` | exit at arithmetic PC; interpreter raises the exact message |
| Float NaN order | `ucomisd` + `jp`/`setnp`, never raw `jb`/`jbe` | branch as Lua (false for NaN) |
| Table shape | `Table*`, `asize`, `lsizenode`, `node`, metatable/flags, array tag, node key | exit at access PC; `luaV_finishget`/`finishset` |
| Metamethod presence | raw slot present for get; present or `checknoTM` for set; primitive tags for arith/compare | exit at opcode; interpreter calls the metamethod in exact order |
| Closure/upvalue | entry `cl` is `LClosure`; reload `upvals[B]->v.p` after safepoints | exit at `GETUPVAL`/`SETUPVAL` |
| Stack move | no cached `base`; reload after every helper | — |
| Hooks/traps | `L->hookmask==0 && ci->u.l.trap==0` at entry and each backedge | exit at loop header; interpreter runs hooks |
| String compare | both strings; `strcoll` semantics, not `memcmp` | exit at compare PC |

**Blacklisting and caps.** On abort, increment the `(Proto*, pc)` abort counter; after 5 aborts blacklist the loop for the flush epoch; `jit.flush()` clears
blacklists. Caps: `maxrecord` 4000 bytecodes, `maxir` 1000 instructions, `maxmcode` 512 KiB, `maxtrace` 1000. A capped trace is discarded, never partially
emitted; `maxrecord` also bounds recorder time. Each exit counts failures; after `hotexit` (default 10) failures record a side trace from that exit with the
spilled state. Link the exit stub to the side trace only when tags match; cap depth (default 5) and link a would-be cycle back to the parent loop. J1–J3 have no
side traces; J4 adds them.

## 6. GC integration

Lua's GC scans a thread's stack only up to `th->top.p` (`src/lgc.c:684-719`). A trace may not hold live collectable values only in registers at a GC point. At
every safepoint: spill all live values and set `L->top = ci->top.p` (frame top), as `savestate` does (`src/lvm.c:1153-1167`). The GC then sees a normal frame.
No register stack map is needed because canonical locations are slots; all unboxed values are materialised before a helper that can trigger GC.

**Write barriers.** Do not omit barriers because a table was just allocated: the allocation helper can run GC/finalizers, so the table may be black afterwards.
Table stores emit `luaC_barrierback` semantics (`src/lgc.h:251-255`, `src/lgc.c:268-280`); upvalue stores emit `luaC_barrier` (`src/lgc.h:245-249`); stack-slot
stores need none (the stack is a root and the thread is re-traversed, `src/lgc.c:695-706`). Lua 5.5's array part stores tags separately (`getArrTag`/`obj2arr`,
`src/ltable.h:108-145`), so a trace store writes the exact tag byte and payload, then the value barrier. Barrier helpers do not allocate or call Lua, but may
relink the table into `grayagain`.

**Allocation helpers.** `NEWTABLE`, `CLOSURE`, future string helpers and `luaH_resize`-driven `SETLIST` call the same VM functions the interpreter uses
(`luaH_new`, `luaH_resize`, `luaF_newLclosure`/`luaF_findupval` as in `pushclosure`), never a reimplementation. They spill, set `L->top` to cover live slots, set `savedpc` to the
emulated instruction (`p+1`) for error lines, and on return are a full memory barrier: reload `base`, invalidate table/upvalue facts, re-check shapes. An OOM
raises `LUA_ERRMEM` via `luaD_throw` (`src/ldo.c:125-147`), longjmps out of the trace and leaves the stack canonical; there is no generated cleanup. Finalizers
may run arbitrary Lua, including `jit.off()/flush()`; after the helper the trace continues only with reloaded/guarded values.

**Lifetime and roots.** A trace keeps its `Proto` alive; the global JIT state holds the trace list, and the GC root scan marks each trace's `Proto`
(`restartcollection`, `src/lgc.c:440`); the Proto marks its constants/nested protos (`traverseproto`, `src/lgc.c:646-660`). Code may embed `Proto->k` addresses
because the Proto is non-moving and rooted. Traces do not embed arbitrary `Table*`/closure pointers; those are loaded from slots and guarded. `jit.flush()`
frees traces/code only when `active == 0`; otherwise it sets a pending flush performed by the last exiting trace. `close_state` calls `luar_jit_shutdown` before
`luaC_freeallobjects`, so finalizers at close run interpreted and cannot execute freed code (`src/lstate.c:260-273`).

**Generational GC.** Lua 5.5 runs incremental, generational-minor or generational-major (`KGC_INC`/`KGC_GENMINOR`/`KGC_GENMAJOR`, `src/lstate.h:160-164`). The
barriers above are exactly the VM barriers, so old→young pointers created by a trace are recorded. `isblack`/`iswhite` are byte tests; never cache a colour
across a safepoint. Fresh objects are anchored in the stack before further allocation, so a minor collection sees them.

## 7. Recording

Add tagged `/* LUAR */` fields to `Proto` (`src/lobject.h:603-626`): `uint16_t luar_hot[8]; uint8_t luar_hot_epoch;`, indexed by `(pc - p->code) & 7`.
Collisions only cause an extra recording attempt at a less hot loop. On a backedge, `luar_jit_hot()` checks `G(L)->luar_jit`, the enabled flag and the epoch;
stale counters reset to `hotloop` (default 56). `jit.opt.start("hotloop=N")` sets the threshold; `hotloop=0` disables tracing. `jit.flush()` bumps the epoch,
lazily resetting counters without walking the heap.

> Open question: the 8-slot array adds 17 bytes to every `Proto` and collides. A lazily allocated `uint16_t *hot` sized to `sizecode` is exact but needs a free
> hook in `luaF_freeproto`. Measure `Proto` size and `bench/` before J2.

A trace is keyed by `(Proto*, startpc)` plus an entry type signature. When a backedge counter reaches zero, record. If a trace exists for the key, try it (entry
guards validate the signature); repeated entry exits record another specialisation (max 2–4) or blacklist. The loop root is always the header the interpreter is
about to execute.

**The recorder is the execution.** It is not a shadow interpreter: it executes the loop body for real with real VM helpers while emitting memory-form IR. Every
result is written to its canonical slot immediately, so an abort is trivial: discard IR, leave the stack, return to the interpreter at the current PC. The
recorded iteration is not re-executed when the trace starts; the recorder already performed it, and compiled code begins with the next iteration. Errors during
recording propagate normally; a helper that moves the stack forces a `base` reload. The IR is initially `SLOAD`/`SSTORE` for every operand/result; after the
loop closes, mem2reg promotes slots to SSA registers, inserts `PHI`s at the header and inserts spills at safepoints/exits. Recorder correctness stays obvious;
the aggressive transformation is a separate testable pass.

**Aborts.** Unsupported opcode; reached `MMBIN*`; an invoked metamethod; `CALL`/`TAILCALL`/`RETURN` (J1–J3); `VARARG`/`GETVARG`/`VARARGPREP`; `CLOSE`/`TBC`;
`ERRNNIL`; top-dependent layout; a table miss needing `__index`/`__newindex`; a type change; a nested backedge to another loop (J1–J3); hook/trap; yield;
`jit.off()`; caps; a stack move invalidating a shape; a `Proto`/upvalue/closure mismatch. Count by reason in `luar.jit.stats()`; 5 aborts blacklist the loop
until flush.

**Nested loops, recursion, coroutines.** J1–J3 record exactly one root loop; an inner backedge aborts the outer recording and gets its own hot counter. J4 may
record an inner loop in the outer trace or as a side trace. Traces are keyed by `Proto`/PC, not frame, and all accesses are `base`-relative, so one trace serves
recursive invocations; recursive calls themselves exit (call opcode). JIT state lives in `global_State`; only one thread executes at a time. The recorder stores
`lua_State *rec_owner` and the active `CallInfo`; a yield or thread switch aborts recording. No trace resumes after a yield: in J1–J3 yieldable operations are exits; a J4 helper may call `luaD_call`, but a yield unwinds the trace and the interpreter resumes. This
is per-thread recorder state in effect without adding a field to `lua_State`; entry checks the owner and `L->status == LUA_OK`. TBC variables are never open
across a trace: `TBC`, `CLOSE`, and `RETURN` with `k` exit before closing; a loop inside a `<close>` scope is traceable while its body avoids those opcodes.

> Open question: parallel Lua states sharing a code cache need locking and per-state creation queues. Out of scope for J1–J5.

## 8. Code generation

x86-64 SysV first. DynASM is vendored at `dynasm/` (`dynasm.lua`, `dasm_x86.lua`, `dasm_proto.h`, `dasm_x86.h`, 1.5.0), with its backends ported to native Lua 5.5 operators. Write `src/luar_emit.c`
plus `src/luar_emit_x64.dasc`; check the generated `src/luar_emit_x64.h` in and add a `make dynasm-check` target that regenerates and diffs. This avoids
requiring a built Luar to build Luar (bootstrap cycle).

Trace ABI: `uint32_t luar_trace_run(lua_State *L, CallInfo *ci, StkId base, LuaR_Trace *t)`. `r14=L`, `r13=ci`, `r15=base` (reloaded after helpers),
`r12=trace`, `rbx=Proto->k`, `rbp` frame; scratch `rax,rcx,rdx,rsi,rdi,r8–r11`; `xmm0–xmm15` caller-saved. Slot `A` is `[r15+16*A]`, payload `+0`, tag byte
`+8`. Prologue pushes `rbp,rbx,r12–r15`, aligns `rsp` to 16 before `call`, loads args, falls into the first instruction; epilogue restores and returns the exit
id in `eax`. Never use the red zone (signals).

Constants are `[rbx + 16*idx]`; 64-bit immediates use `movabs`. Tags compare with `cmpb $TAG, 8(slot)`; a failing guard is `jne exit_stub_id`. Each exit has an
out-of-line stub that stores its spill map, sets `eax=exit_id` and jumps to the epilogue. Side-trace stubs first compare the entry signature, then jump to the
side trace or return.

Helpers follow SysV: spill, set `savedpc`/`top`, `rdi=L`, args in `rsi/rdx/rcx/r8/r9`, `call`, reload `base`, invalidate facts. Helpers have fixed signatures in
`src/luar_jit.h`; callee-saved registers survive by ABI, so `L/ci/base/trace` stay valid or are reloaded.

`mmap(RW)`, emit, `mprotect(RX)` (W^X); on ARM64 clear the icache after writing. macOS hardened runtime needs `MAP_JIT` and `pthread_jit_write_protect_np`
around writes. Track code pages per trace and unmap on flush; debug builds `mprotect(PROT_NONE)` after flush to catch stale calls.

**ARM64/macOS follow-up.** Reuse IR/metadata; add `src/luar_emit_arm64.dasc`. `x0=L`, `x1=ci`, `x2=base`, `x3=trace`; `x19–x28` callee-saved, `x9–x15` scratch,
`x16/x17` veneers; `v0–v7`/`v16–v31` scratch; SP 16-byte aligned. Constants via `adrp/add` or a literal pool; guards `cmp`/`b.ne`; helpers `blr`. Test Linux
arm64 and Apple Silicon; macOS requires `MAP_JIT`. Estimate 4–8 weeks after the x86-64 MVP, mostly ABI and guard-stub work.

## 9. FFI fast paths (phase 0.7)

The FFI lands before the JIT (roadmap 0.2); this is the JIT contract for it and is **provisional** until `src/luar_ffi*.c` exists (today `src/luar_ffi.h` only
reserves the registry key).

* **cdata loads/stores.** Recognize a cdata pointer/reference with a known `CType*`; guard ctype pointer, base non-NULL, bounds and alignment; emit a native
  1/2/4/8-byte load/store. Aggregates/16-byte copies use moves or a helper.
* **Struct offsets.** `offsetof` is a compile-time constant in the ctype descriptor; embed it as an immediate. Guard ctype identity, not the cdata allocation
  (non-moving).
* **libffi bypass.** For a static signature classify args/returns as INTEGER or SSE per SysV; emit argument moves, stack args, 16-byte alignment and hidden
  structure-return handling. Fall back to libffi for varargs, aggregates by value, callbacks and anything not proven. The `ffi.C` symbol address is resolved at
  record time and embedded; guard the symbol/ctype, not the address.
* **Failure paths.** Bounds/alignment failures exit; the interpreter raises the same FFI error. A C call is opaque (callbacks are a 0.2 non-goal): full safepoint,
  assume all memory changed.
* **Allocation sinking** for cdata created/discarded inside a trace is a J5 stretch goal; require a local non-escape proof, and never sink across calls, yields or
  exits.

> Open question: the cdata layout (`GCcdata`, `CType`, array/struct descriptors) is not implemented yet. Revise the offsets/guards against
> `src/luar_ffi_internal.h` before J5.

## 10. Integration plan and file layout

| File | Contents |
| --- | --- |
| `src/luar_jit.h` | `LuaR_JIT`, `LuaR_Trace`, `LuaR_Exit`, `IRIns`, hooks, `luar_jit_hot` inline, no-JIT stubs. |
| `src/luar_jit.c` | `jit.*` control surface/lifecycle: `on/off/flush/status/opt/dump/stats`, trace list, active depth. |
| `src/luar_trace.c` | Hot counters, selection, recorder, abort/blacklist, trace cache, flush epoch. |
| `src/luar_ir.c` | IR construction, mem2reg/optimiser, dumper, test-only IR interpreter. |
| `src/luar_emit.c` | DynASM x86-64 backend, guard stubs, prologue/epilogue, code memory, helper ABI. |
| `src/luar_emit_x64.dasc`, `src/luar_emit_x64.h` | DynASM source and checked-in generated header (`make dynasm-check`). |
| `tests/jit/*.lua` | Differential, guard-fuzz, dump and stress cases. |

**Narrow core hooks** (all `/* LUAR */`, no reordering/whitespace churn):

1. `src/lobject.h` `Proto` (~626): `luar_hot[8]`, `luar_hot_epoch`.
2. `src/lstate.h` `global_State` (~380): `struct LuaR_JIT *luar_jit;`.
3. `src/lstate.c` `lua_newstate`/`close_state`: zero it; call `luar_jit_shutdown` before `luaC_freeallobjects`; free after.
4. `src/lvm.c` backward `OP_JMP` (`GETARG_sJ(i) < 0`) after `dojump` (1652), `OP_FORLOOP` after `pc -= GETARG_Bx` (1847), `OP_TFORLOOP` after `pc -=` (1904): one hot check and `pc = luar_jit_backedge(...)`, then `updatebase(ci)`. This is the whole interpreter coupling.
5. `src/lgc.c` `restartcollection` (440): mark every trace's `Proto` with `markobjectN` (small loop over `g->luar_jit->traces`). No new GC type and no `Proto` free
   hook.
6. `src/Makefile` (`WITH_JIT`, JIT objects, DynASM check), `src/luar.h`/`src/luar_lib.c` (`luar.features.jit`, `luar_has_jit()`).

`src/lvm.c` includes `luar_jit.h`; without `LUAR_HAS_JIT` the inline hook is a no-op, so the interpreter builds/runs alone. No `#ifdef` in opcode bodies.

`WITH_JIT` defaults to 1 on x86-64 Linux/macOS, 0 elsewhere in J1. `src/Makefile` adds `$(JIT_O)` to `LUAR_O`, defines `-DLUAR_HAS_JIT`, and compiles
`luar_emit.c` with the generated header. `libluar.a` contains the JIT; embedding needs no extra libraries. `make WITH_JIT=0` stays green and `jit.status()`
reports false.

**Reviewable drift.** JIT logic lives in `luar_*`; the core has only field declarations, three backedge hooks, the GC root loop and lifecycle calls (target
under ~60 changed lines). Each hook is tagged and self-contained; no opcode implementation changes. After any core edit, run `tools/upstream-check.sh --update`,
review the diff against `vendor/lua-5.5.1/src/`, and commit the baseline update in the same logical change (`doc/upstream.md`). CI runs the upstream check and
the official suite with JIT on/off. A new opcode changes `NUM_OPCODES` in `src/lopcodes.h`; the recorder's `switch` has `default: abort`, so it cannot silently
miscompile.

**Making `jit.*` real.**

| API | Behaviour after J2 |
| --- | --- |
| `jit.on()` | `enabled=1` for this `global_State`; entry allowed again. |
| `jit.off()` | clears `enabled`; no new entry; traces remain allocated but unreachable until flush. |
| `jit.flush()` | bumps epoch, discards traces/blacklists; frees code only when `active==0`, else defers. Optional function/Proto/trace-id argument later. |
| `jit.status()` | `true, flags` when built and enabled; `false, flags` otherwise. Flags are stable CPU strings (`"x64"`, later `"arm64"`), never addresses. |
| `jit.opt.start(...)` | parses `hotloop`, `hotexit`, `maxrecord`, `maxir`, `maxmcode`, `maxtrace`; unknown options accepted/ignored with a warning; no options resets defaults. |
| `jit.dump(...)`, `luar.jit.stats()` | deterministic IR/exit dumps; traces/aborts/exits/blacklist/mcode/hits. Additive, not LuaJIT `jit.util`. |

`luar.features.jit` is true only when codegen is compiled in. `jit.status()` never claims codegen on a build without `WITH_JIT`. **Flush safety:**
`luar_trace_run` increments `active` on entry and decrements on exit; `jit.flush()`/`jit.off()` from a finalizer during an allocation helper only sets
`flush_pending`; the last exiting trace frees. This prevents freeing code that is still executing.

## 11. Testing

**Differential interpreter-vs-trace harness.** New `tests/jit/` suite; each case is deterministic Lua. `tests/jit/run.lua` runs each case in two subprocesses
with `LUAR_JIT=0` and `LUAR_JIT=1`, with `jit.flush()` between phases, and compares stdout/stderr/exit status byte-for-byte. Cover errors (`pcall` and
uncaught), metamethod traces, coroutines, weak tables, `__gc` and `debug`; a second mode runs with `collectgarbage("step")` after every allocation to shake out
barriers.

**Dumps and golden IR.** `jit.dump("ir")` writes stable textual IR with PC/exit mapping, normalised trace ids and no addresses; golden files under
`tests/jit/golden/` catch unintended changes. `jit.dump("exit")` records `(trace, exit id, pc, reason)`; tests assert the expected exit on a perturbed
type/shape.

**Randomized guards.** `tests/jit/guards.lua` generates integers at int64 edges, floats (NaN/±0/±inf), short/long strings, tables with changing
shapes/metatables, nil/booleans, and calls small loops under both modes. It changes tags/shapes between iterations to force exits, then compares with the
interpreter. Fixed PRNG seed, printed on failure; every new guard gets a case that flips it.

**IR oracle.** `luar_ir_exec` runs recorded IR against a copy of the canonical stack and compares with the VM on the same inputs. This is the roadmap 0.4
differential replay and catches IR/mem2reg bugs before machine code exists.

**Selection/flush tests.** Hot thresholds (`hotloop=1/10/0`), blacklisting, `jit.on/off` toggled mid-run, and `jit.flush()` called from inside a called function
(must defer, not crash). Abort tests assert no semantic change and a reported reason.

**Sanitizers.** `make test-jit-asan` builds core/recorder/IR with `-fsanitize=address,undefined -fno-omit-frame-pointer`. Generated code is not instrumented:
indirect calls into it opt out of UBSan's `function` check, which expects a type word just before the callee entry.
add `LUAR_JIT_DEBUG` emitter assertions (spill-map completeness, stack alignment, exit-id bounds), guard pages around code, and `mprotect(PROT_NONE)` after
flush. UBSan for mem2reg/IR; ASan for recorder and trace metadata. Leak checking stays disabled for the Lua state as in `doc/testing.md`, but trace
metadata/code pages are checked explicitly.

**Benchmarks and official suite.** `bench/bench.lua`/`bench/compare.sh` with stock Lua 5.5.1, Luar JIT off/on, LuaJIT 2.1 on/off; record machine/compiler/date
and losses in `doc/performance.md`; no hard CI gates. J2 gate: numeric ≥3× stock. J3 gate: table/object 1.5–3×. J5 gate: FFI kernel within 30% of LuaJIT with
identical results. Run `tests/upstream/run.sh`, `tests/semantics/` and `tests/diff/` with JIT on and off, and stress both GC modes, weak tables/ephemerons,
finalizers, `<close>`, coroutines and `debug.sethook` (line/count/call/return).

## 12. Milestones J1–J5, effort, risks

| Milestone | Scope | Exit criteria | Effort |
| --- | --- | --- | --- |
| **J1** recorder + IR + dumps | Hot counters, selection, recorder, linear IR dumper, abort/blacklist, stats. No codegen at this stage (J2 adds it). | Stable traces for a numeric-loop corpus; differential replay green; no semantic change; recording overhead bounded and disableable. | 3–5 weeks |
| **J2** x86-64 numeric MVP | DynASM emitter; MOVE/LOAD*/JMP/FORLOOP/FORPREP/ADD/SUB/MUL/comparisons/returns; entry guards, exit stubs, safepoints, barriers; real `on/off/flush/status/opt`. | ≥3× on call-free numeric loops and mandelbrot; `fib` measured and reported but not a J2 gate; semantics + differential + official suite green; flush during execution safe; caps enforced. | 6–10 weeks |
| **J3** tables, upvalues, call exits | Shape/array/hash guards, GET/SET family, SELF, NEWTABLE/SETLIST, GETUPVAL/SETUPVAL, CLOSURE, call-exit paths, side-trace groundwork. | Table/object 1.5–3×; each new opcode has differential + guard-fuzz coverage; no barrier failures under GC stress. | 8–12 weeks |
| **J4** metamethods, side traces, coroutines, TBC | MMBIN-aware recording/exit discipline, side traces/trees, nested loops, call helper for known C callees, coroutine yield exits, CLOSE/TBC exits. | Full opcode-family coverage; coroutine/`<close>` suites green on/off; side-trace depth caps; randomized guards green. | 6–10 weeks |
| **J5** FFI fast paths + hardening | cdata loads/stores, struct offsets, bounds/alignment, libffi bypass for static signatures, allocation sinking (stretch), ARM64/macOS planning. | FFI kernel within 30% of LuaJIT with identical results; ASan/UBSan green; docs, compatibility matrix and performance report updated. | 6–10 weeks |

Total: **29–47 weeks of focused work** (roughly 7–11 months), plus review, porting and stabilisation. This is a multi-month project, not a sprint; J2 alone is
larger than the 0.1 foundation.

**Top five risks**

1. **Semantic drift** (mixed int/float compares, metamethod order, error text/lines, NaN, shifts, `strcoll`). Mitigation: fast-path-only rule (never call a
   metamethod or build an error in a trace), `savedpc`/`pcRel` discipline, differential + guard fuzzing, official suite on/off, review of every opcode against
   `lvm.c`.
2. **GC/lifetime bugs** (missing barrier, freed code still executing, a live object only in a register at a safepoint, flush during a finalizer). Mitigation:
   canonical stack at safepoints, exact VM barriers, rooted `Proto`s, active-depth flush deferral, ASan + GC stress + `mprotect` poisoning.
3. **Stack reallocation/stale pointers** (cached `base`, `upvals->v.p`, table `node`). Mitigation: reload `base` after every helper, no stack pointer across a
   safepoint, shape guards include `node`/`asize`, test that forces stack growth inside a traced loop.
4. **Codegen/ABI bugs** (register clobber, stack misalignment, tag/payload width, W^X/icache, bad spill maps). Mitigation: small fixed helper ABI, DynASM
   discipline, prologue/epilogue tests, IR oracle before machine code, `LUAR_JIT_DEBUG` spill checks, disassembly dumps, differential tests.
5. **Scope/performance disappointment** (call-heavy, FFI, complex tables dominate real workloads). Mitigation: milestones with external exit criteria, honest
   losses in `doc/performance.md`, call/FFI sequenced after a numeric MVP, no LuaJIT-parity claim. The interpreter remains a complete, correct product at every
   milestone.

> Open questions to settle before J2/J3: hot-counter layout (embedded array vs per-PC array); trace retention via rooted `Proto`s vs a trace GC object; whether
> the J4 call helper can rely on `luaD_call`'s yield path or must exit before calls; and the exact async window when `lua_sethook` is called from a signal while a
> trace runs. Each needs a decision and a test, not an assumption buried in code.
