# AGENTS.md -- working rules for the LuaR repository

LuaR is a Lua 5.5-forward runtime: the official Lua implementation is the
semantic authority, and LuaJIT-style performance/extensions are layered on
top.  Read `doc/architecture.md` and `doc/roadmap.md` before non-trivial work.

## Non-negotiable rules

- **Never fork the language.** Lua 5.5.1 semantics, bytecode format, C API
  and module ABI come from `vendor/lua-5.5.1/`.  Behavioural changes that
  diverge from upstream Lua need an explicit decision note in `doc/`.
- `vendor/` is read-only reference material.  Edit `src/` only.
- Keep core edits small and marked `/* LUAR */` so the upstream sync stays
  reviewable.  New functionality belongs in new files (`src/luar_*.c`).
- **No new runtime dependencies** without a decision note.  Optional
  libraries must be opt-in at build time (`WITH_FFI=1`, etc.).
- LuaJIT compatibility means API shape and observable semantics, not bug
  compatibility.  Prefer correctness; document every intentional deviation.
- Do not add Lua 5.1 APIs (`unpack`, `module`, `setfenv`, ...) that Lua 5.5
  removed.  LuaR targets 5.5 and newer, not 5.1.

## Build and test

```sh
make                      # luar, luarc, libluar.a
make WITH_FFI=1           # once the FFI milestone is in
make test                 # smoke + semantics + compat suites
make test-ffi             # FFI suite (WITH_FFI=1)
make bench                # microbenchmarks, no hard gates
tools/upstream-check.sh   # src/ vs vendored upstream drift
```

Every behaviour change needs a test under `tests/<suite>/`.  Tests are plain
Lua files that raise on failure; `tests/run.lua` runs them.  Keep test
output deterministic and independent of wall-clock time.

## Layout and ownership

| Path | Contents |
| --- | --- |
| `src/luar_lib.c` | identity table, `luar_openlibs`, table extensions |
| `src/luar_jit.c` | `jit.*` control surface |
| `src/luar_bit.c` | 32-bit LuaJIT-compatible bit library |
| `src/luar_ffi*.c` | FFI (parser, ctype registry, cdata, calls) |
| `src/lua*.c/h` | Lua 5.5 core, modified in place and tracked upstream |
| `dynasm/` | vendored DynASM, the future JIT assembler |
| `tests/` | semantics, compat, ffi, cli suites |
| `bench/` | benchmark harness and corpus |
| `doc/` | architecture, roadmap, compatibility, upstream policy |

When several agents work in parallel, stay inside your assigned files; do
not reformat or move unrelated code.

## Style and commits

- C99, 2-space indent, Lua core style (see `src/lua.c`).
- No tabs in new Lua files; match surrounding style in C.
- Commit messages: `area: imperative summary` (e.g. `ffi: parse struct and
  enum declarations`).  One logical change per commit.
- Report handoffs with: files changed, exact commands run, test output,
  known limitations.
