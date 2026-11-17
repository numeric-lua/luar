# LuaR architecture

Status: 0.1.0. This document describes the system as built and the design
constraints for everything that comes next. It is normative for contributors:
if code and this document disagree, one of them is a bug.

## 1. What LuaR is

LuaR is a Lua runtime with two identities that LuaJIT could never reconcile:

1. **It is Lua.** The language, standard library, bytecode format, C API and
   module ABI are those of the current official Lua release (5.5.1 today,
   5.6 when it ships). There is no second dialect frozen at an old release.
2. **It is fast.** LuaJIT's practical wins — a compact FFI, cheap table
   primitives, a tracing JIT — are provided as additive layers.

Non-goals: compatibility with any legacy Lua line (5.1–5.4), source-level
compatibility with LuaJIT's C API (`lua_State` layout, `LUAJIT_VERSION`),
reproducing LuaJIT bugs, or supporting two Lua language versions at once.
LuaR supports the newest Lua line and moves forward.

## 2. Why the Lua core is the foundation, not LuaJIT

The alternative — reuse LuaJIT's legacy engine and graft Lua 5.5 on top —
was rejected after examining both codebases.  LuaJIT's VM has no integer
subtype and none of 5.5's declaration or closing semantics; its bytecode
and IR assume its own NaN-boxed value model and dual numbers; its FFI is
coupled to its GC, ctype registry and recorder.  Reproducing 5.5's exact
integer, metamethod and `<close>` behaviour there would rewrite every layer
and would keep the project anchored to an old language.

Starting from Lua 5.5.1 gives exact semantics for free, keeps the C API and
module ABI stable, and makes upstream adoption a merge problem rather than a
porting project.  The cost is that the JIT must be built for Lua 5.5
bytecode instead of reused.  That is the work this project exists to do.

See `doc/adr/0001-lua55-core.md`.

## 3. Value model and ABI

LuaR keeps the official 16-byte tagged `TValue`: `int64_t` integers,
`double` floats, interned strings, tables, closures, userdata, threads, and
a `nil`/boolean tag. This is deliberate:

* 64-bit integers cannot be NaN-boxed without a boxed escape hatch, which
  would penalise exactly the numeric code LuaR targets.
* The C API and binary module ABI are defined in terms of this layout.
  Keeping it means **C modules compiled for Lua 5.5 load and run unchanged**
  (`tests/cli.sh` proves this with a stock `pkg-config lua5.5` module).
* The JIT can still unbox integers and doubles into registers between
  safepoints; it just does not need a new heap representation.

The JIT will therefore be a *memory-based* tracing JIT: Lua stack slots
remain the canonical location of values, traces unbox and keep hot values in
registers within a trace body, and spill back to stack slots before any
safepoint (call, allocation, metamethod, guard exit). This is less radical
than LuaJIT's fully register-allocated traces and substantially cheaper to
make correct against an unmodified VM.

## 4. Runtime layers

```
+-------------------------------------------------------------+
| Lua source / precompiled 5.5 chunks (luarc, string.dump)    |
+-------------------------------------------------------------+
| Lua 5.5 front end: lexer, parser, code generator (pristine) |
+-------------------------------------------------------------+
| T0  Lua 5.5 register VM (computed goto), exact semantics    |
| T1  hot-loop counters + trace recorder (milestone 0.4)      |
| T2  SSA IR + optimiser + DynASM x86-64/ARM64 (milestone 0.5)|
| T3  FFI fast paths, allocation sinking, side traces (0.6+)  |
+-------------------------------------------------------------+
| stdlib + luar/jit/ffi extensions | GC | coroutines        |
+-------------------------------------------------------------+
```

* **T0 is the semantic authority.** Every optimisation must be
  observationally equivalent to T0, including metamethod order, integer
  wrap-around, error messages and GC behaviour.
* **T1 is the recorder**, not a compiler: it counts loop backedges, selects
  traces, and records the executed bytecode path plus type feedback.  J1 is
  implemented: hot counters, selection, bytecode snapshots, `jit.stats()`
  and `jit.trace()`; execution is still entirely T0.
* **T2 compiles traces.** Guards fall back to the interpreter at the exact
  bytecode PC; there is no deoptimisation of compiled frames, because
  trace state is always representable as interpreter stack + PC.
* **T3** removes helper-call overhead for FFI-heavy code, the main use case
  of the numeric-Lua ecosystem.

The interpreter is never removed. `jit.off()`, guard exits, unsupported
opcodes, blacklisted loops and `jit.flush()` all return to T0.  The detailed
design — opcode map, IR, guards, GC safepoints, code generation and the
J1–J5 milestones — is in `doc/jit-design.md`; a working DynASM prototype of
the 5.5-bytecode-to-native path lives in `experimental/tracejit/`.

## 5. Bytecode and front end

`luarc` emits the official Lua 5.5.1 bytecode format, including the version
byte and endianness checks. LuaR does not add opcodes; extensions are library
level. This keeps `luarc`/`luar` interchangeable with `luac`/`lua` 5.5.1 and
keeps the JIT's input surface equal to upstream's.

The only front-end changes are cosmetic: program name, banner, default module
paths, and the `LUAR_INIT` variable. They are marked `/* LUAR */` for the
upstream sync.

## 6. Extension surface

Extensions must be additive and detectable:

| Name | Kind | Notes |
| --- | --- | --- |
| `luar` | global table | version, lua_version, os, arch, features |
| `jit` | global table | LuaJIT control surface; `status()` is truthful |
| `ffi` | global table | LuaJIT-compatible core, opt-in (`doc/ffi.md`) |
| `table.new`, `table.clear` | functions | LuaJIT/OpenResty names; `table.create` is upstream |

Rules:

* Never shadow or change an upstream 5.5 function with incompatible
  behaviour. Add new names instead.
* Every extension must be usable through `require()` and, where LuaJIT had
  it, as a global. `package.loaded` and `_G` agree.
* Extensions that cannot work yet must fail loudly or report the truth:
  `jit.status()` returns `false` until code is actually generated, and
  `luar.features.ffi` is `false` in builds without libffi.
* There is no legacy-language compatibility layer.  Code that relies on
  pre-5.5 APIs must be updated; LuaR never emulates an older Lua.

## 7. Following upstream without losing the fork

Two trees are maintained:

* `vendor/lua-X.Y.Z/` — pristine upstream tarball contents. Never edited.
* `src/` — the LuaR engine, derived from the vendor tree.

The deltas in `src/` are tracked by `tools/upstream-check.sh` against a
checked-in baseline (`tools/upstream-baseline.txt`). A new Lua release is
adopted by: fetch + verify the tarball, replace `vendor/`, three-way diff
into `src/`, re-apply LuaR additions, run the semantic suite and the official
Lua test suite, update version macros and paths, benchmark, release. The full
checklist lives in `doc/upstream.md`.

The design keeps this cheap:

* extensions live in new files (`src/luar_*.c`), not scattered `#ifdef`s;
* core edits are small and tagged `/* LUAR */`;
* the C ABI does not change, so binary modules keep working across LuaR
  releases that track the same Lua minor;
* the JIT will hook `lvm.c` through a narrow, documented interface rather
  than editing opcode implementations throughout the file.

When Lua 5.6 arrives, LuaR 1.x adopts it and drops 5.5 from the supported
line, exactly as the project promises. `LUAR_LUA_VERSION` and the
`luar/5.5` module paths are the version-specific knobs.

## 8. Modules and the ecosystem

`package.path`/`package.cpath` search LuaR directories before the
corresponding Lua ones:

```
/usr/local/share/luar/5.5/?.lua
/usr/local/share/lua/5.5/?.lua
/usr/local/lib/luar/5.5/?.so
/usr/local/lib/lua/5.5/?.so
```

So a module can ship a LuaR-specific build without breaking plain Lua, and
plain 5.5 modules work unmodified. C modules are ABI-compatible because the
core is the official core; no shim layer is involved.

`libluar.a` is the default artifact. `make shared` additionally builds
`libluar.so.0.1.0` with SONAME `libluar.so.0` and the `libluar.so.0` /
`libluar.so` symlinks. The shared objects are compiled separately as
`.pic.o` files, so the static build's objects and archive are untouched and
both configurations can coexist in `src/`.

The shared library exports exactly the Lua 5.5 C API (`lua_*`, `luaL_*`,
`luaopen_*`) and the `luar_*` additions. `etc/libluar.map` is an anonymous
ELF version script that hides the ~200 internal `luaX_*` helpers which have
external linkage in the core. An anonymous node filters the dynamic symbol
table without adding symbol versions, so ordinary C modules resolve
normally. The SONAME is `libluar.so.0`, deliberately distinct from
`liblua.so.5.5`: the ABI is Lua 5.5's, but the library carries LuaR
extensions and may grow more, and its name must not collide with a system
`liblua` in the same process. A stock-`lua` module loaded by a LuaR host
still binds its `lua_*` references to `libluar.so`, because the host's own
dependency precedes the module's `liblua` dependency in the lookup scope.

`luar.pc` describes both link modes: `Libs` is enough for a dynamic link,
while `pkg-config --static` adds `Libs.private` (`-ldl`, needed by the
`dlopen`-based module loader on older glibc). `make test-shared` links an
embedding host and the CLI against `libluar.so` and loads a stock Lua 5.5 C
module into it. The ELF/GNU-ld path is the tested one; the Makefile also has
a minimal macOS `-dynamiclib` fallback, but it has no export list and no CI
coverage yet.

## 9. Testing strategy

1. **Semantic conformance** (`tests/semantics/`): the same files must pass
   under stock Lua 5.5.1 and under LuaR. This is the anti-fork guard.
2. **Official suite** (`tests/upstream/`): the Lua authors' own tests,
   run against LuaR, with a report.
3. **Extension tests** (`tests/compat/`, `tests/ffi/`): API shape,
   semantics, failure modes.
4. **ABI/CLI tests** (`tests/cli.sh`, `tests/shared.sh`): build/load a stock
   Lua 5.5 C module, link an embedding host against both `libluar.a` and
   `libluar.so`, check bytecode round-trips, environment and exit codes.
5. **Differential JIT tests** (from 0.5): every trace is compared against
   the interpreter on randomized inputs before it is allowed to run.
6. **Benchmarks** (`bench/`): no hard gates, but every release records
   numbers versus Lua 5.5.1 and LuaJIT 2.1, including losses.

## 10. Risks

| Risk | Mitigation |
| --- | --- |
| Upstream changes internals used by the JIT | narrow hook surface, tagged diffs, sync CI |
| JIT correctness bugs corrupt the VM | interpreter remains authoritative, differential tests, guard-exit design |
| FFI ABI mistakes cause memory corruption | libffi for calls, bounds-checked cdata access, ASan test job |
| Extension creep recreates a second dialect | compatibility contract, additive-only rule, feature matrix |
| Performance claims drift from reality | benchmark docs with conditions and losses reported |

## 11. Decision records

* `doc/adr/0001-lua55-core.md` — Lua 5.5 core over grafting onto LuaJIT.
* `doc/adr/0002-libffi.md` — libffi for FFI calls, opt-in at build time.
* `doc/adr/0003-upstream-deltas.md` — vendor snapshot + baseline discipline.
