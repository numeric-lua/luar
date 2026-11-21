# Allocation and GC-safe objects in J4 traces

Status: design/decision note, 2026-11-21.  No implementation is included in
this change.  This note is normative for the first allocation work in J4.
Baseline: the 0.57.0 J4 core.  0.58.0 adds the unrelated `CCALL3DD`
native-call form and 0.59.0 adds the rooted J4 entry wrapper; neither
changes the arguments below.  Line anchors are approximate (0.58.0/0.59.0
tree); function and macro names are authoritative.

The J4 tracing IR is documented in `doc/j4-tracing-ir.md`; the interpreter
remains the semantic authority (`doc/architecture.md`, `doc/jit-design.md`).
This note does not restate the whole tracing design.  It answers one
question: how can a J4 trace create collectable objects without weakening
the correctness contract?

**0.69.0 update.**  `ULOAD` now handles any non-function upvalue as well:
the recorder emits the load followed by a tag guard, so numeric/table
upvalues can participate in traces while type changes exit cleanly.
Function/cdata upvalues keep the identity-only path used by native calls.

**0.68.0 update.**  `GETUPVAL` is now traceable through `LUAR_IR_ULOAD`,
which carries the expected `LClosure` pointer and upvalue index.  The
runtime helper verifies the current closure identity before copying the
upvalue, so a trace cannot read another closure's upvalue.  This closes the
top-level `local abs = ffi.C.abs` native-call gap.

**0.67.0 update.**  Constant-string table fields are traceable:
`GETFIELD`/`SETFIELD` lower to `TGETK`/`TSETK` with the interned `TString`
pointer from the Proto constant table.  Reads use `luaH_getstr`; writes use
`luaH_set` with a value copy and return the relocated base.  This covers
`{x = i}` and `t.x = i; ... t.x`.

**0.66.0 update.**  Constant-index table access is traceable:
`GETI`/`SETI` lower to `TGETI`/`TSETI`; reads use the array part with a
`luaH_getint` fallback and never allocate, writes reuse the relocation-safe
insert helper.

**0.65.0 update.**  Sized `NEWTABLE` and `SETLIST` are traceable:
array literals such as `{i}` allocate through `TNEW` (array/hash hints in
`ival`/`nval`), then write through `TSETI`; the table is published to its
stack slot before `luaH_resize`.  Constructor sizes with the `k` extra-arg
form remain interpreter-side.

**0.64.0 update.**  A traced `TNEW` table can be filled through the
existing `TSET` path and read back in the same loop; `tests/jit/75` covers
the combination, including GC and `alloc=0` fallback.

**0.63.0 update.**  `luar_trace_tset_insert` now copies the value before
calling `luaH_setint` and returns the (possibly relocated) frame base to the
emitter, which reloads `rbx`.  A hash/array growth that steps the GC can
therefore move the Lua stack without leaving the trace using a stale base.
The helper still allocates, so a general allocation safepoint remains
future work.

**0.62.0 update.**  The first allocation opcode is implemented:
`LUAR_IR_TNEW` allocates one empty table behind `jit.opt.start("alloc=1")`.
It uses `luaH_new` without `luaC_checkGC`, publishes the table to its stack
slot under the rooted `L->top` protocol, and is restricted to full root
traces with empty `NEWTABLE`.  General allocation, `SETLIST`, `ffi.new`,
side-trace allocation and sinking remain out of scope.

**0.61.0 update.**  `LuaR_JIT` now tracks `active_depth`; `jit.flush()`
seen while a native trace runs sets `flush_pending` and the outermost
`luari_run_j4` return performs `free_code()` only after generated code is
off the C stack.  `tests/jit/73_flush_deferred.lua` exercises finalizers
requesting a flush during traced TSET growth.  The first allocation opcode
still requires the rest of §6 (non-allocating TSET split and `TNEW`).

**0.59.0 update.**  J4 trace entry now wraps every native call in
`luari_run_j4` (`src/luar_jit.c`), which raises `L->top` to cover the whole
frame while the trace runs and restores it on return.  The TSET-growth GC
point described in §1.2 is therefore rooted today, and
`tests/jit/72_trace_tset_gc.lua` stresses it.  This does not make TSET a
supported allocator, does not add deferred `jit.flush()`, and does not close
the callback re-entry hole; those remain prerequisites for the first real
allocation opcode.

The wrapper also has a stack-move caveat that matters as soon as an
allocation helper runs a normal GC step.  `luari_run_j4` saves the incoming
top as a raw `StkId` and restores that pointer; if a helper calls
`luaC_checkGC` and the stack is reallocated, the saved pointer is into the
old stack.  The interpreter's local `base` is likewise not reloaded after
`LUAR_JIT_RUN`.  This is mostly latent today because the TSET-growth path
does not call `luaC_checkGC` (only an emergency allocation failure can move
the stack), but it must be fixed in phase 0.2 before `TNEW` calls
`luaC_checkGC`.

Short answer: allocation is a **safepoint**, not an ordinary arithmetic
operation.  A trace may allocate only through a small VM-owned helper that
publishes the result to a canonical stack slot, with a full root set and
write barriers, and only while the JIT can prove that a flush cannot free
the executing code.  The first implementation should allocate one empty
table (`TNEW`) and nothing else.

## 1. Current state

### 1.1 What the IR and the executor can do

`LuaR_TraceIR` holds up to 64 instructions and 4 exits
(`src/luar_trace.h:17-21`).  The opcode list is arithmetic, table
read/write, FFI scalar access/calls, guards, `FORLOOP`, `EXIT`, `RET`, and
the terminal `LUAR_IR_CALLEXIT` (`src/luar_trace.h:23-74`).  There is no
allocation opcode.

Generated root code has the shape `int f(void *base, lua_State *L)`: `rbx`
holds the current `TValue` base, `r12` holds `L`
(`src/luar_emit_x64.dasc:1458-1467`).  Every IR result is written to its
stack slot immediately; only short-lived codegen temporaries live in
registers.  `LUAR_IR_CALLEXIT` jumps to the exit stub with the CALL PC
(`src/luar_emit_x64.dasc:1855-1860`).  The JIT hook decodes the negative
return into an exact interpreter PC (`src/lvm.c:1169-1173`).

The current call/continuation design is important context:

* A full trace stops recording at a CALL that is not one of the whitelisted
  inlined forms and emits `LUAR_IR_CALLEXIT`
  (`src/luar_trace.c:2151`, `:2379`).
* On a CALL exit the interpreter executes the CALL normally.  For a C
  callee only, `OP_CALL` then calls `luari_jit_continue`, which may run a
  separately recorded suffix starting at `callpc + 1`
  (`src/lvm.c:1275`, `:1826`; `src/luar_jit.c:2899-2916`).
* A Lua callee replaces the current frame and never returns through that
  hook, so a Lua CALL is a full interpreter exit, not a native
  continuation.
* The suffix is a second trace with its own entry guards.  It does not
  share registers or a `CallInfo` with the prefix.  `LuaR_TraceIR` has
  `partial` and the `Proto` fields `luar_cont_*` for this path
  (`src/luar_trace.h:99-109`; `src/lobject.h:640-658`).

The JIT state currently does not have an active-entry depth or a deferred
flush.  `jit.flush()` bumps the epoch and calls `free_code()` immediately
(`src/luar_jit.c:3578-3587`; `free_code` at `:81`).  That is safe only
while no trace helper can run arbitrary Lua/GC; it is a prerequisite to
fix before adding a GC-capable helper.  The same is true of `jit.off()`
from a finalizer: the old code remains mapped until flush, but a flush
would unmap it.

### 1.2 What a trace may allocate today

**By design: nothing.**  The FFI trace helpers are explicitly
allocation-free (`src/luar_ffi_trace.c:1-7`), and the inlined leaf-call
forms copy a few arithmetic instructions, not a general call.  `NEWTABLE`,
`SETLIST`, `CLOSURE`, `CONCAT`, and every other allocation-capable opcode
fall into the recorder's `default: return 0`
(`src/luar_trace.c:2393`), so the loop stays interpreted.

**By implementation: one unintended path exists.**  The emitted `TSET`
uses `luar_trace_tset_insert` (`src/luar_emit_x64.dasc:1762-1775`), which
calls `luaH_setint(L, t, key, val)` (`src/luar_trace.c:342-366`).  If the
key is outside the array part and has no existing hash cell,
`luaH_setint` can reach `luaH_newkey`/`luaH_resize` and allocate.  That is a
real GC point inside a live trace.  It is not a supported allocation
facility, and it is not counted as one.  It is a pre-existing gap that the
first allocation patch must close: either make `TSET` reject absent keys
(and exit to the interpreter), or give it the full allocation protocol in
this note.  The first implementation should do the former.

The other `TSET` helpers write only integer values.  They write the array
tag and payload directly (`src/luar_trace.c:390-406`) and therefore need no
`luaC_barrierback` today: integers are not collectable.  That shortcut ends
the moment a trace stores a table, string, closure, or cdata.

**C-call callback audit.**  A separate call-side escape is worth fixing
before any allocator: the zero-argument C-call classifier accepts any
`CDATA_FUNC` cdata and does not reject `CDATA_CALLBACK`
(`luar_ffi_trace_ccall0_kind`, `src/luar_ffi_trace.c:556-578`), and
`ffi_trace_call0` itself checks only the magic and signature
(`src/luar_ffi_call.c:2632-2675`).  A `ffi.callback` cdata invoked in a
traced loop would therefore re-enter Lua through libffi, with no frame-top
root protocol and no active depth.  Some other C-call forms already reject
`CDATA_CALLBACK`; the whitelist is not uniform.  Every C-call kind and
helper must reject callbacks explicitly.  This is a pre-existing
call-safety hole, not an allocation feature; it must not be left for the
allocation patch to discover.

### 1.3 Allocation matrix (0.57.0-0.59.0)

| Trace path | Lua heap allocation | Status |
| --- | --- | --- |
| Integer/float arithmetic, `FORLOOP` | no | trace-side |
| `TGET`/`TGETF`, existing array or hash cell | no | trace-side; helper uses `luaH_get*`, which does not allocate |
| `TSET` existing array cell or existing non-nil hash cell | no | trace-side; integer values only, no barrier needed |
| `TSET` absent out-of-array key | **can allocate** via `luaH_setint` | trace-side gap; fix before adding allocators |
| FFI `CGET*`/`CSET*`/`CCALL*` | no Lua allocation | trace-side; helpers revalidate ctype/signature; scalar signatures only |
| Inlined leaf Lua call | no | trace-side; arithmetic whitelist only |
| Any other CALL | yes, on interpreter side | `CALLEXIT`; C call may get a suffix continuation |
| `NEWTABLE`, `SETLIST`, `CLOSURE`, `CONCAT` | yes, on interpreter side | not recorded; default abort |
| `ffi.new` | yes, on interpreter side | ordinary CALL today; a possible continuation target later |
| `__index`, `__newindex`, `__eq`, `__lt`, `__close`, `__gc` | yes, on interpreter side | traces exit before the effect |

The distinction matters for every later section: "interpreter-side" means
the trace has already exited and the normal VM owns the stack, roots, and
errors; "trace-side" means generated code and a C helper are still on the
C stack.

## 2. GC facts the design must respect

### 2.1 Roots are the thread stack up to `top`

`traversethread` marks stack elements only up to `th->top.p`
(`src/lgc.c:695-705`).  `restartcollection` marks the main thread, the
registry, and the global metatables (`src/lgc.c:440-447`).  There is no JIT
root scan and no hidden trace root set: `src/lgc.c` contains no
`luar_jit` reference.  Trace metadata lives on `Proto`, and a live
`LClosure` keeps its `Proto` alive (`traverseLclosure`, `src/lgc.c:672-680`),
but generated code itself is only reachable through that `Proto`'s fields.

Consequences:

* A collectable only in a C local, a machine register, or a `LuaR_IRExitState`
  is **not** a root.
* Before any helper that can trigger GC, `L->top` must cover every stack slot
  that holds a live collectable.  A trace entered at a loop backedge cannot
  assume the interpreter's `L->top` is already at the frame top; `OP_FORLOOP`
  and `OP_JMP` do not raise it.  The safe wrapper is to set
  `L->top = base + p->maxstacksize` for the duration of the native call and
  restore the incoming top afterwards.  0.59.0 implements this shape in
  `luari_run_j4`, but saves the incoming top as a raw `StkId` (see the
  stack-move caveat above); phase 0.2 must make that offset-based before an
  allocation helper calls `luaC_checkGC`.
* Restoring the incoming `top` at trace exit is correct because the
  interpreter re-establishes the top it needs at the next instruction (and
  before any call/`Protect`).  Restoring it while still *inside* the trace is
  wrong: a later helper would then run with a too-small root set.

### 2.2 Stack reallocation invalidates every pointer

`luaD_reallocstack` moves the stack and corrects registered pointers
(`src/ldo.c:330-354`).  An allocation helper can therefore move `base`, any
`TValue *`, any `StkId`, and any `UpVal->v.p` that points into the stack.
After every GC-capable helper the trace must reload `base` (or use the new
base returned by the helper) and re-read values from slots.  No `Table *`,
`TString *`, `LClosure *`, `CData *`, or `UpVal *` may be cached across such
a helper.  Cdata headers are not moved, but a pointer to one is only valid
while the object is rooted.

Reloading inside the trace is not sufficient.  `luaV_execute` keeps `base`
in a C local, and the `LUAR_JIT_RUN` hook does not update it after the
native call (`src/lvm.c:1944-1956` is the `OP_FORLOOP` site; the `OP_JMP`
and continuation sites have the same shape).  If a trace helper moved the
stack, the interpreter would continue with a stale `base`; every hook that
can enter an allocation-capable trace must call `updatebase(ci)` after the
native call.  `luari_jit_try_run` must likewise reload `base` before a side
trace attempt.

### 2.3 Barriers

The VM's barriers are in `src/lgc.h:241-252`: `luaC_barrier` for a
single-object forward barrier (upvalue stores use it), and
`luaC_barrierback` for tables.  The interpreter's table stores call
`luaC_barrierback` after the raw store (`src/lvm.c:354`, `:377`, `:2046`).
Stack-slot stores need no barrier because the stack is re-traversed.

Rules for trace-side allocation:

* A table store of a collectable value must call `luaC_barrierback` after
  the store, even if the table was allocated earlier in the same trace.  A
  GC step in an allocation helper can blacken a fresh table; "it is new" is
  not a barrier proof.
* An upvalue store must call `luaC_barrier` after the store.
* A stack store never needs a barrier, but it also does not make the value a
  root unless the slot is covered by `top`.
* Use the VM's `luaH_set`/`luaH_setint` only where its allocation behavior is
  understood.  For a non-allocating fast store, use the existing raw array
  path only for non-collectable values.  For collectable values, use a
  helper that stores and then barriers.

### 2.4 Errors, finalizers, and flush

Allocation can raise `LUA_ERRMEM` through `luaD_throw`, which longjmps out
of generated code (`src/ldo.c:125-147`).  Generated code has no cleanup:
the stack must already be a valid interpreter stack when the helper is
called, and the helper must publish or not publish atomically.  There is no
partially initialized object visible to the error handler.

GC can run finalizers.  `GCTM` runs them with `luaD_callnoyield`
(`src/lgc.c:968-1010`), so they cannot yield, but they can run arbitrary Lua,
allocate, move the stack, and call `jit.flush()`/`jit.off()`.  The active
depth and pending-flush rule is therefore not optional.  A finalizer that
calls `jit.flush()` must not unmap the code currently on the C stack; the
last active trace performs the pending free after it returns.

### 2.5 Cdata is userdata

`ffi.new` creates a full userdata through `lua_newuserdatauv`, with a
`CData` header and two user values (`src/luar_ffi.c:99-115`).  The GC marks
its metatable and user values (`traverseudata`, `src/lgc.c:631-638`); the
owner reference used by reference cdata is one of those user values.
`cdata_gc` may call a `__gc` finalizer or free callback state
(`src/luar_ffi.c:1831-1869`).  A cdata value is rooted only by the stack
(or by another rooted object such as a table).

`ffi.new` is not a primitive allocator: it converts initializers, may use
`lua_geti`/`lua_getfield` on a table initializer
(`src/luar_ffi.c:487-506`, `:587-611`), and can therefore invoke
`__index` and run arbitrary Lua.  That makes the full `ffi.new` a bad first
in-trace allocator.  A whitelisted zero/constant-initialized form can be
added later, after the table case proves the protocol.

## 3. Invariants

These are the conditions every trace-side allocation must satisfy.  They
are deliberately stronger than "the tests pass".

I1. **Canonical state.**  Every live Lua value is in a stack slot before a
GC-capable helper is called.  No collectable value is held only in a
machine register, a C local, or an exit snapshot across a safepoint.

I2. **Root coverage.**  Before a GC-capable helper, `L->top` covers the
frame's live slots (`base + maxstacksize`, or an equivalent proven bound).
The incoming interpreter top is restored when the native call returns.

I3. **Atomic publication.**  An allocation helper either publishes the new
object into its destination slot and returns success, or fails before
publishing and returns failure.  It never returns failure after an object
has become observable.  On success, the destination slot is the only
authority; no shadow copy is needed.

I4. **Reload after helpers.**  `base`, stack pointers, and every cached
collectable pointer are reloaded after any helper that can allocate, raise,
run a finalizer, or otherwise call Lua.  Table/closure/cdata facts cached
before such a helper are invalid.

I5. **Exact barriers.**  Any store of a collectable value into a table or
upvalue uses the same barrier the interpreter uses, after the store.  No
barrier is omitted because an object is "fresh".

I6. **Whitelist.**  Only opcodes explicitly listed by the current phase may
allocate.  The recorder aborts rather than emitting a speculative partial
allocation.  Unsupported shapes fall back to the interpreter.

I7. **Flush safety.**  Any GC-capable trace helper runs with a positive
active-entry depth.  `jit.flush()`/`jit.off()` from a finalizer only raises
a pending flag; code is unmapped after the last active trace returns.  A
trace whose epoch changed does not re-enter.

I8. **Snapshots are not roots.**  `LuaR_IRExitState` is metadata/selftest
state.  It must not be used as a root set and must not be restored over live
stack slots after a GC-capable helper.  Exit state is `(pc, top, canonical
slots)`, not a copy of values.

I9. **One authority.**  The interpreter can always resume at the exact
bytecode PC with a normal frame.  There is no compiled-frame
deoptimization, no heap rollback, and no second copy of the stack.

I10. **No new identity roots.**  Do not extend the current `CGUARD`
address-immediate scheme to new collectable kinds without a stable
non-collectable identity (for example `CType *` or `Proto *`) or an explicit
exit.  An address embedded in generated code does not keep an object alive.

## 4. Phased model

The phases are ordered by risk, not by user-visible value.  Each phase is a
separate installable change with its own tests; no phase may be partially
emitted.

### Phase 0 - prerequisites (no allocation yet)

0.1. Add an active-entry depth and a pending-flush flag to `LuaR_JIT`.
     Enter/leave every J4 entry (root, side, continuation), and make
     `jit.flush()` free code immediately only when the depth is zero.
     A finalizer is allowed to call `jit.flush()`/`jit.off()`; it is not
     allowed to unmap the running trace.  This is a correctness fix even
     for the existing `TSET` allocation path.
0.2. Make the 0.59.0 top/root wrapper stack-move safe.  Save the incoming
     top as a stack offset (`savestack`/`restorestack`), not a raw `StkId`,
     and reload `base` after the native call.  Add `updatebase(ci)` (or an
     equivalent reload) at every `LUAR_JIT_RUN`/continuation site in
     `src/lvm.c` and before a side-trace attempt in `luari_jit_try_run`.
     The current wrapper restores a raw pointer and leaves the interpreter's
     `base` local untouched; that is acceptable only while no trace helper
     moves the stack.
0.3. Make `TSET` non-allocating for the current supported shapes.  The
     simplest safe rule is: record `TSET` only when the interpreter's fast
     path would store into an existing array cell or an existing non-nil
     hash cell; any absent/growing key exits before the store.  This removes
     the accidental allocation and is independently testable.
0.4. Add `ntop`/top metadata to the exit path only if a trace changes
     `L->top`.  If phase 0.2 restores the top on every return, root traces
     do not need it; continuations and side segments may, because their
     incoming top differs from the loop entry top.
0.5. Add a debug assertion (`LUAR_JIT_DEBUG`) that the root set covers
     `base + maxstacksize` before every helper that may GC, and that the
     active depth is positive.

Phase 0 is a prerequisite, not a user-visible feature.  It should land and
be tested before `TNEW` is enabled.

### Phase 1 - empty table allocation (`TNEW`)

This is the narrow first implementation (section 5).  A trace may execute
`OP_NEWTABLE` for an empty table with no initializer and no writes to the
new table inside the trace.  The object is an ordinary Lua table created by
`luaH_new`; no new GC type, no new heap representation.

### Phase 2 - pre-sized tables and table stores

Extend `TNEW` to the `NEWTABLE` B/C sizes (`luaH_resize` after
publishing), then add stores of collectable values:

* `TSET` of a collectable value with `luaH_set`/`luaH_setint` plus
  `luaC_barrierback`; growth is a safepoint.
* `SETLIST` with preallocation, one barrier per collectable value, and
  resume points that do not replay completed stores.
* `TGET`/`LEN` on the new table may follow once the shape guards are
  understood; they do not allocate.

The important design point is ordering: publish the table to its slot
*before* resizing or filling it.  A resize or a later allocation may run
GC; the table must be rooted while that happens.  A store is committed only
after the value and tag are written and the barrier has run.  If a helper
fails after a committed store, the exit PC must be after that store; a
bytecode PC that would re-execute it is not a valid resume point unless the
store is idempotent and has no observable intermediate state.

### Phase 3 - strings and closures

Strings:

* `luaS_newlstr`/`luaS_new` allocate a `TString`; the result must be stored
  to its slot immediately.  Short strings may return an existing interned
  string; do not assume a fresh pointer.
* `CONCAT` is a multi-operand VM helper that can allocate and call
  `__concat`; keep it interpreter-side until the raw-string-only path and
  its top/exit semantics are implemented.  A trace must not inline
  `__concat`.
* A string stored into a table still needs `luaC_barrierback`; strings are
  marked as values and are never weak, but the barrier applies to the
  collectable value.

Closures:

* `CLOSURE` is `pushclosure` in `lvm.c`: create the `LClosure`, publish it
  to the destination slot, then create/find the upvalues
  (`src/lvm.c:873-900`).  An open upvalue aliases a stack slot and its
  `v.p` changes if the stack moves; reload it after every safepoint.
* The closure result is rooted by its slot.  A store into a table/upvalue
  uses the normal barrier.  A closure with upvalues is not a simple leaf;
  do not reuse the existing leaf-inline machinery for it.
* `CLOSURE` can allocate several objects and can raise; it is a full
  safepoint, not a single instruction with a cheap helper.

### Phase 4 - cdata allocation and `ffi.new`

There are two independent strategies; they should not be conflated.

**4a. Interpreter-side `ffi.new` via call exit (safe, limited).**
`ffi.new` is an ordinary C function.  The existing prefix/CALL-exit/suffix
machinery already lets the interpreter perform the allocation and then
runs a native suffix.  What is missing is a useful guard for the *fresh*
cdata: the current `CGUARD` compares the recorded cdata address, and a new
cdata from `ffi.new` will normally have a different address.  Add a
type/kind guard (stable `CType *`, element size, owned/reference flag)
before using the result in `CGET*`/`CSET*`.  Until then, a continuation may
move/store the cdata but any cdata access should exit.

This is the recommended way to support `ffi.new` in hot loops before a
trace-side allocator exists.  It keeps all initializer conversion,
metamethods, `__gc`, and error handling in the interpreter.

**4b. Trace-side cdata allocation (later, whitelisted).**
A `CNEW` helper may support a fixed whitelist:

* scalar cdata, zero or a recorded constant initializer;
* fixed-size arrays of scalar elements, zero-filled;
* structs/unions of known size, zero-filled;
* no table/string/cdata initializer, no `ffi.metatype` `__new`, no
  variadic/VLA unless the length is an integer operand with a bound check;
* no `ffi.gc` finalizer installation inside the trace.

The helper uses the same `lua_newuserdatauv`/`CData` header path as
`ffi_push_cdata_object`, publishes the userdata to the slot before any
further allocation, sets the metatable, and calls `luaC_checkGC`.  It must
not call the Lua-facing `ffi_new` function, because that function reads
arguments from the Lua stack and may invoke metamethods.

The cdata type identity for guards should be the interned `CType *`
(the FFI arena lives as long as the `FFIState`, which is rooted by the
registry) rather than the cdata address.  The `CData->S` back-pointer is
stable for the same reason, but generated code should not cache it across
helpers.

### Phase 5 - general allocation and sinking (not planned here)

General `luaD_call`-style allocation, allocation of objects with
metamethod-sensitive initializers, and allocation sinking are out of scope.
Sinking needs an escape proof that is stable across calls, exits, yields,
and GC; J4 does not have that analysis.  A sunk object must never be
observable through a finalizer or a weak table, which is exactly the case
that is hard to prove.

## 5. Narrow first implementation: `TNEW` for empty tables

### 5.1 Scope

Add one IR kind, `LUAR_IR_TNEW`, and one VM helper.  The recorder emits it
only for `OP_NEWTABLE` when all of the following hold:

* the table is empty: `GETARG_vB(i) == 0`, `GETARG_vC(i) == 0`, and the
  `TESTARG_k` extra-argument form is not used;
* the destination slot is valid and within the trace's slot bound;
* no `SETLIST`, `SETTABLE`, `SETI`, or `SETFIELD` appears later in the
  recorded range.  The first patch is intentionally conservative: any
  table write after the allocation aborts the whole trace.  This keeps
  `TSET` growth and collectable stores out of the first patch;
* the trace is a root trace (`side == 0`).  Side traces and continuations
  containing `TNEW` are rejected at install time in phase 1;
* the trace is not a partial/CALL prefix (`t->partial == 0`).  A `TNEW`
  before a terminal `CALLEXIT` is rejected, so a continuation cannot observe
  a table created by its prefix.

Everything else falls back exactly as today.  `ir=1` remains the gate;
add `alloc=1` (default off) as a second opt-in so a build can ship the
machinery without enabling it.  `jit.opt.start("ir=0")` or `alloc=0` must
leave the interpreter path unchanged.

### 5.2 IR shape

```
LUAR_IR_TNEW   a = destination slot; ival = 0; nval = 0
```

The first patch has no operands because the table is empty.  The verifier
checks the destination slot.  The type/liveness pass marks the destination
as non-numeric (`kind[a] = 0` in `luar_trace_ir_analyze`), and
`ir_integer_only`/`ir_floatify` must not treat the trace as numeric-only
without considering `TNEW`.  The emitter must not fold or reorder across
`TNEW`.

### 5.3 Helper contract

```c
/* Returns the current frame base after a possible stack move, or NULL if
** the helper rejected the request before allocating anything. */
TValue *luar_trace_tnew (lua_State *L, TValue *base, int slot);
```

Implementation sketch:

1. Validate `L`, `base`, and `slot < L->ci->top - base` (or an equivalent
   frame check).  Return `NULL` on failure; no allocation has happened.
2. Require the phase-0 wrapper to have already set `L->top` to cover the
   frame.  Assert that invariant; do not shrink `top` inside the helper, or
   the next allocation would lose roots.
3. `Table *t = luaH_new(L);`
4. Publish immediately: `sethvalue2s(L, current_base + slot, t);`
5. Optionally `luaC_checkGC(L);` (the helper may allocate, so it must
   participate in GC pacing the way `OP_NEWTABLE` does).
6. Return the current base.  If the stack moved, the returned pointer is
   authoritative; the generated code writes it back to `rbx`.

The helper never calls `luaH_resize` in phase 1, so there is no second
allocation after publication.  The table has no metatable and no contents.

Emitter changes:

* call the helper with `rdi = L` (currently `r12`), `rsi = base` (`rbx`),
  `rdx = slot`;
* on `rax == NULL`, set `r11d = pc` and take the normal exit path;
* on success, `mov rbx, rax` before the next instruction.  Do not reload
  `base` from `L->ci` in generated code; the helper returns the corrected
  pointer, which avoids exposing `CallInfo` offsets to codegen.
* The destination slot's tag is written by `sethvalue2s`; no separate tag
  store is needed.

### 5.4 Exit and deoptimization semantics

* `TNEW` has no exit of its own.  If the helper rejects before
  allocating, the emitter exits at the `NEWTABLE` PC; the interpreter
  executes the instruction and allocates normally.
* If the helper succeeds, the table is already in the slot.  A later guard
  exit resumes after the completed instruction (the normal exact-PC exit),
  not at the `NEWTABLE` PC.  The table is not rolled back; it is a normal
  live value if the resumed bytecode still refers to that slot, otherwise
  it is garbage, exactly as an interpreter-created temporary would be.
* If `luaH_new` raises OOM, `luaD_throw` unwinds; the stack is canonical
  and no retry occurs inside the trace.
* No snapshot is taken for `TNEW` and no snapshot is restored after it.
  The slot is the state.

### 5.5 Non-goals for the first patch

Explicitly **not** in the first implementation:

* `SETLIST`, `SETTABLE`, `SETI`, `SETFIELD`, `TGET`, or `LEN` on the new
  table;
* pre-sized `NEWTABLE` (`B`/`C` nonzero) and the `EXTRAARG` form;
* table writes of any kind after a `TNEW` in the same trace;
* strings, `CONCAT`, closures, upvalues, and `CLOSURE`;
* `ffi.new`, cdata allocation, `CNEW`, and `ffi.gc`;
* side traces, chained side traces, or continuations containing `TNEW`;
* allocation sinking, escape analysis, scalar replacement, or
  object promotion;
* weak-table or finalizer special cases;
* exact allocation counts, GC step timing, or `collectgarbage("count")`
  equivalence;
* ARM64 or non-x86-64 backends;
* any change to J2/J3 loop emitters or intrinsics;
* any new GC type, heap layout, or C API.

The first patch may make some loops slightly slower.  That is acceptable;
it is a correctness milestone, not a performance milestone.

## 6. Failure and fallback rules

F1. **Unsupported shape.**  The recorder returns 0 and the loop stays
interpreted.  It must not emit a partial `TNEW` plus a fallback inside the
same trace.  Count the rejection (for example `ir_alloc_rejects`) without
changing `ir_aborts` semantics for existing tests.

F2. **Runtime guard.**  A failing guard exits at the exact bytecode PC
before the guarded effect.  Values already published by earlier
instructions remain; the interpreter sees a normal frame.

F3. **Helper rejection before publication.**  The helper returns `NULL`,
no object exists, and the emitter exits at the instruction PC.  The
interpreter re-executes the instruction.

F4. **Error after publication.**  By construction this must not happen
for `TNEW`.  For later phases, a helper that can fail after publishing must
define a resume PC after the committed effect; otherwise the design is
invalid.  OOM is an error (`luaD_throw`), not a fallback: there is no
in-trace cleanup.

F5. **Stack move.**  Reload `base` from the helper's return value (or from
`L->ci` after a general call); re-read every cached slot.  Any cached
collectable pointer is invalid.

F6. **Hook set during a helper.**  Finalizers run with hooks disabled, but
they can call `debug.sethook` for later.  After a GC-capable helper, if
`L->hookmask != 0`, leave the trace at the next safe PC; the interpreter
owns hooks.

F7. **Flush/off during a helper.**  The active depth keeps the code mapped;
the pending epoch is checked before the next trace entry.  A trace that
continues after a pending flush must not read torn metadata or enter a side
trace compiled for the old epoch.

F8. **Side trace/continuation allocation.**  Phase 1 rejects install.  A
later phase must run the same top/root/active protocol and must not restore
a pre-allocation snapshot over live slots.

F9. **GC mode changes.**  Allocation helpers call the VM allocator, so
incremental, generational-minor, and generational-major modes keep their
own invariants.  Do not cache colors, ages, or `gcstate` across a helper.

F10. **No silent success on unsupported FFI shapes.**  A fresh cdata from
`ffi.new` is not the recorded cdata.  Any `CGET*`/`CSET*`/`CCALL*` that
needs an identity it does not have must exit, not assume the recorded
pointer.

## 7. Snapshots, exit state, and deoptimization

### 7.1 What exists

`LuaR_IRExitState` stores a resume PC, a slot count, and 64 tag/8-byte
payload pairs (`src/luar_trace.h:92-97`).  `luar_trace_ir_snapshot` copies
live slots; `luar_trace_ir_restore` writes them back
(`src/luar_trace.c:225-241`).  The header explicitly says a snapshot is
short-lived and must not cross a GC safepoint
(`src/luar_trace.h:118-123`).  That is correct: the copies are not roots.

At runtime, J4 root/side exits are PC-based.  The J2 side path uses a
snapshot/restore pair around a side attempt
(`src/luar_jit.c:3173-3183`).  That is safe only while the side code cannot
allocate; it is not a model for allocation deoptimization.

### 7.2 Rules

* **Do not add `top` implicitly.**  If phase 0.2 restores the incoming top
  at the native boundary, root traces keep the interpreter top.  A
  continuation may start with a different top than the loop entry; record
  and restore the top explicitly for that path rather than assuming.
* **No runtime value copies.**  The canonical slot is the only state.
  `LuaR_IRExitState` may remain useful for selftests and diagnostics, but
  not as a runtime spill buffer for allocation.
* **No restore after allocation.**  Once a helper can allocate, a
  pre-helper snapshot can contain pointers to objects that die while the
  side code runs.  Restoring it would resurrect dead pointers.  Delete or
  gate the restore path before enabling allocation in any side/continuation
  code.
* **Exact PC, not frame reconstruction.**  A trace owns no `CallInfo`.
  Deoptimization is `ci->u.l.savedpc = p->code + pc`, a valid `L->top`, and
  the canonical slots.  Every allocation op must have a well-defined resume
  PC as described in section 5.4.
* **Spill maps only if registers are introduced.**  The current emitter
  writes each result to its slot immediately.  If a future register-cached
  trace keeps collectables in registers, it must spill them before any
  helper and before any exit; that is a separate design, not a prerequisite
  for `TNEW`.

### 7.3 `CGUARD` is not a root

The existing `CGUARD` embeds a closure/cdata address as an immediate
(for example `src/luar_trace.c:1106`; emitter at
`src/luar_emit_x64.dasc:1789-1793`).
Generated code never dereferences the immediate, and the FFI helpers
revalidate the ctype/signature, but the immediate does not keep the object
alive and can be reused by a later allocation.  It is an identity filter,
not a root or a stable type guard.  New allocation work must not extend
this scheme to fresh cdata or fresh closures.  Use a stable non-collectable
identity (`CType *`, `Proto *`) or exit.

## 8. Test and benchmark plan

### 8.1 Regression tests for the prerequisites

1. **`TSET` GC stress.**  Fill a table with integer keys that force
   growth and GC while the trace runs; keep live stack values above the
   interpreter top and assert they survive.  This is the regression for the
   accidental `luaH_setint` allocation and for the top wrapper.  A
   deterministic test can use a fixed loop count and `collectgarbage`
   settings chosen before the loop; no wall-clock dependence.
   `tests/jit/72_trace_tset_gc.lua` is the 0.59.0 starting point; extend it
   with live locals above the frame top rather than only checking the table.
2. **Stack move during a helper.**  With `WITH_FFI=1`, install a cdata
   finalizer that calls a recursive/deep Lua function, drop the cdata, and
   force GC from a `TNEW` helper.  The stack must move under the trace;
   assert the incoming `top` is restored as an offset and the interpreter's
   `base` is reloaded.  Without this test, the raw-`StkId` restore in
   `luari_run_j4` can pass every existing test and fail only under a
   finalizer that grows the stack.
3. **Flush from a finalizer.**  With `WITH_FFI=1`, create a cdata with
   `ffi.gc(obj, function() jit.flush() end)`, drop it, then run an
   allocating traced loop.  The flush must defer; no crash and no stale
   code execution.  Skip without FFI.
4. **Hook interaction.**  Set a debug hook while a finalizer runs; the
   trace must leave at the next safe PC and the hook must see the normal
   interpreter events.

### 8.2 `TNEW` tests

Use the existing `tests/jit/` style: deterministic, `H.eq`/`H.check`, skip
when `jit.status() ~= true`, and restore `jit.opt.start("ir=0","alloc=0")`
before printing `ok`.

* **`62_ir_alloc_newtable.lua`**: a loop with `local t = {}` and a live
  assignment; assert the result is a table, `next(t) == nil`, the trace was
  compiled/executed, and `ir_alloc_ops`/`ir_alloc_executed` are positive.
* **`63_ir_alloc_gc.lua`**: the same loop with GC pressure; keep live
  table/string values in locals above the frame top and assert they are
  unchanged after the loop.  This catches missing roots and stale `base`.
  With FFI, add a cdata finalizer that grows the stack during the `TNEW`
  helper so the test also covers the stack-move path.
* **`64_ir_alloc_exit.lua`**: force a guard exit after `TNEW` (change a
  non-table operand type between iterations); assert the interpreter
  result is identical to `jit.off()` and that no table is lost or
  duplicated in an observable way.
* **`65_ir_alloc_fallback.lua`**: `{1,2,3}`, `t[1] = 1` after `{}`,
  pre-sized `NEWTABLE`, and a side-trace attempt must all be rejected and
  produce the interpreter result.  Assert the rejection counter, not just
  the value.
* **`66_ir_alloc_flush.lua`**: `jit.flush()` between phases and re-run;
  no stale code, results unchanged.
* **`67_ir_alloc_selftest.lua`** (or extend `jit.trace_ir().selftest()`):
  verifier accepts a valid `TNEW`, rejects an out-of-range destination,
  and the emitter refuses `TNEW` in a side segment.
* **Differential**: run `62`-`66` in two subprocesses with JIT off/on and
  compare stdout/stderr/exit status byte-for-byte, as the existing JIT
  harness does.
* **ASan/UBSan**: `make test-asan`, including the new tests; generated
  code stays uninstrumented as today, but helper/metadata paths are
  covered.
* **Build variants**: `make JIT=0 test`, `make WITH_FFI=0 test`,
  `make WITH_FFI=1 test`, and `make test` must stay green.

### 8.3 Benchmarks

Add a `j4/newtable_empty` group in `bench/bench.lua` with
`skip = j4_skip` and an `alloc=1` gate.  It should:

* allocate and discard one empty table per iteration;
* keep a live last table to prevent the interpreter from optimizing the
  loop away (it will not, but the trace should see an escape);
* compare interpreter, JIT with `alloc=0`, and JIT with `alloc=1`.

Expected result: allocation-bound; no speedup, possibly a loss.  Record it
in `doc/performance.md` with machine/compiler/date and GC mode, as the
project already does for losses.  Do not add a hard gate.

Later groups: `j4/newtable_sized`, `j4/table_fill_alloc`,
`j4/ffi_new_exit` (interpreter-side `ffi.new` plus continuation), and
`j4/ffi_new_cnew` (trace-side, if phase 4b lands).  For every group report
`ir_alloc_ops`, `ir_alloc_executed`, `ir_alloc_rejects`, `ir_code_bytes`,
and whether the loop stayed on the interpreter.

### 8.4 What the tests must not assert

Exact allocation counts, `collectgarbage("count")`, GC step timing, or
finalizer order between unrelated objects.  The contract in
`doc/jit-design.md` explicitly excludes those.  Tests should assert
reachability, values, error messages/lines, and absence of crashes/leaks
under stress.

## 9. Performance risks

1. **Safepoint overhead.**  Setting/restoring `top`, calling a helper,
   reloading `base`, and invalidating cached facts on every allocation can
   cost more than the allocation itself in tight loops.  `TNEW` is
   inherently allocation-bound; do not sell it as a speedup.
2. **GC pacing changes.**  Calling `luaC_checkGC` from the helper makes GC
   observe the same allocation debt as the interpreter, but at a different
   program point.  That is allowed by the contract; it can still change
   cache behavior and measured time.
3. **Extra roots retain memory.**  Covering the whole frame at every
   safepoint can keep dead temporaries alive longer than the interpreter
   would.  Keep the window as short as possible and restore the exact top
   on return.
4. **Barrier cost.**  Collectable stores need `luaC_barrierback` even for
   fresh tables.  A trace that allocates and fills a table can approach
   interpreter cost; the win comes only from eliminating dispatch, not
   from skipping barriers.
5. **Code size and the code cache.**  Each allocation adds a helper call,
   a null check, a `mov rbx, rax`, and exit plumbing.  `LUAR_JIT_MAXCODE`
   is 64 mappings and `maxmcode` is 512 KiB; allocation-heavy functions
   can hit caps earlier.  Rejecting side traces for allocation (phase 1)
   also removes potential wins.
6. **Loss of specialization.**  A fresh cdata has a different address each
   iteration, so address-based `CGUARD` is useless and exits.  Type-based
   guards add checks; without them, `ffi.new` continuations have little
   value.
7. **No allocation sinking.**  Escaping allocations remain real heap
   objects.  Do not expect LuaJIT-style scalar replacement; J4 does not
   have the escape analysis and this note does not add it.
8. **Flush/epoch overhead.**  Active-depth increments on every trace entry
   are cheap, but pending-flush checks and epoch checks add branches to the
   hot path.  Keep them out of the generated loop body where possible.

## 10. Open questions

1. **Top contract at loop backedges.**  Is `L->top` ever above the frame
   top at a J4 entry?  The phase-0 wrapper makes it irrelevant for safety,
   but the exact restore point for continuations needs measurement.
2. **Active-depth placement.**  A field in `LuaR_JIT` is simplest because
   the JIT state is per `global_State`.  It must be incremented for root,
   side, child, and continuation entries, including nested calls from a
   finalizer.
3. **`TNEW` resume metadata.**  Phase 1 needs no per-op resume state
   because it has no post-publication failure.  Phase 2 (`SETLIST`,
   growth) does; define it before implementing phase 2.
4. **Cdata type guard.**  Is the interned `CType *` stable for the whole
   state lifetime, including after `ffi.cdef` additions?  The arena is
   owned by `FFIState`; verify that interning never moves or frees a
   `CType` while a trace is installed.
5. **`CGUARD` replacement.**  Address immediates are convenient but not
   roots.  Decide whether the next change replaces them with
   `Proto *`/`CType *` identities or keeps them as a filter and relies on
   helper revalidation.  Do not expand their use before deciding.
6. **Side-trace snapshots.**  The current restore path must be removed or
   gated before allocation reaches side traces; the first patch avoids it
   by rejecting `TNEW` in side segments.  The 0.59.0 top wrapper touches
   the same entry points, so the snapshot/restore path must be updated in
   the same phase-0 change that makes top restoration offset-based.

## 11. Decision summary

* Allocation is a safepoint with a VM helper, not a new arithmetic op.
* The stack slot is the only publication point; no shadow roots, no
  rollback, no snapshot restore after GC.
* Fix the accidental `TSET` allocation and the callback re-entry hole, add
  active-depth/deferred flush, and make the 0.59.0 top/root wrapper
  stack-move safe before enabling any allocator.
* First implementation: `TNEW` for empty tables only, root traces only,
  behind `ir=1,alloc=1`; explicit non-goals as listed in section 5.5.
* `ffi.new` stays interpreter-side via CALL exit/continuation until a
  stable cdata type guard exists; trace-side cdata allocation is a later,
  whitelisted phase.
* Strings/closures, table fills/growth, side-trace allocation, and
  allocation sinking are later phases, each with its own invariants and
  tests.
* No performance claim: allocation-bound loops are expected to be neutral
  or slower; report the loss.
