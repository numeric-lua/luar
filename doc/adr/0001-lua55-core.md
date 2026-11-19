# ADR 0001 — Build on the Lua 5.5 core, not on LuaJIT

Date: 2026-11-17
Status: accepted

## Context

Luar must provide LuaJIT-class performance and FFI while tracking the current
Lua language. Two starting points were considered:

1. Fork LuaJIT 2.1 and teach it Lua 5.5 semantics.
2. Fork Lua 5.5.1 and build LuaJIT-class features on top.

LuaJIT's interpreter, GC, FFI and JIT are built around a 5.1 value model with
no integer subtype. Lua 5.5 requires exact 64-bit integers, integer
wrap-around, `<const>`/`<close>`, `global` declarations, read-only loop
variables, and a different coercion and metamethod protocol. Those choices
reach into every layer of LuaJIT.

## Decision

Start from the pristine Lua 5.5.1 sources. Keep the official language, C API,
bytecode format and module ABI. Build the following as additive layers:

* LuaJIT-compatible libraries (`bit`, `table.new/clear`, `jit`, `ffi`);
* a tracing JIT over the **Lua 5.5 bytecode**, with the unmodified
  interpreter as the semantic authority and fallback;
* an upstream sync process that treats the official release as the source of
  truth and Luar deltas as a small, reviewable patch set.

## Consequences

Positive:

* exact 5.5 semantics from day one, including subtle cases;
* C modules compiled for Lua 5.5 work unchanged (verified in CI);
* upstream adoption is a merge, not a language port;
* the JIT can be introduced incrementally without a second language dialect.

Negative:

* LuaJIT's existing JIT, IR and FFI cannot be reused wholesale — only design
  ideas and DynASM;
* the JIT must be written for Lua 5.5 bytecode, which is the project's main
  long-term cost;
* Lua 5.1-only modules are not supported.

## Alternatives rejected

* **Graft 5.5 onto LuaJIT** — rewriting the recorder, GC value model and FFI
  ctype integration costs more than a new recorder, and permanently couples
  the project to a dead front end.
* **Embed both runtimes** — two languages in one binary; unacceptable
  semantic ambiguity and ABI surface.
* **Transpile 5.5 to LuaJIT 5.1 bytecode** — cannot preserve integers,
  `global`, `<close>` or error behaviour; debugging becomes a two-language
  problem.
