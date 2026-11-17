# LuaR

**LuaR is a Lua 5.5-forward runtime with LuaJIT's superpowers — and
without LuaJIT's time freeze.**

LuaJIT stopped at Lua 5.1 and stayed there. LuaR starts from the official
Lua 5.5.1 sources, keeps the complete Lua 5.5 language and C API, and adds
the things people actually missed from LuaJIT: a compact FFI, the `jit`
control surface, `table.new`/`table.clear`, 32-bit `bit` compatibility, and
a tiered JIT engine on the roadmap.

The project's defining rule: **LuaR follows Lua.** When Lua 5.6 ships, LuaR
adopts it. There is no second, frozen dialect of the language to maintain.

```
$ luar -v
LuaR 0.1.0 (Lua 5.5.1)  Copyright (C) 2026 LuaR contributors
```

## Status (0.1.0)

| Area | State |
| --- | --- |
| Lua 5.5 semantics and C API/ABI | complete (vendored Lua 5.5.1 core) |
| `luar`, `luarc`, `libluar.a` | working |
| `luar` identity table, `LUAR_INIT`, `luar/5.5` module paths | working |
| `jit` control surface | working, truthfully reports JIT off |
| `table.new` / `table.clear`, `bit` library | working |
| FFI | in progress (`WITH_FFI=1`, milestone 0.2) |
| native JIT engine | designed, not shipped (milestone 0.3+) |

LuaR 0.1 is a drop-in replacement for the `lua` 5.5 interpreter and for
`liblua`: bytecode format, C API and module ABI are those of Lua 5.5.1.
Build it with `WITH_FFI=0` (default) or `WITH_FFI=1` once the FFI milestone
is complete.

## Build

```sh
make                 # src/luar, src/luarc, src/libluar.a
make test            # smoke + semantics + compat suites
make install         # PREFIX=/usr/local by default
```

Requirements: a C99 compiler, `make`, POSIX. FFI builds additionally need
`libffi` (`pkg-config --cflags --libs libffi`).

## Repository layout

```
src/                 LuaR engine (derived from Lua 5.5.1)
  luar_lib.c         identity, table extensions, extended openlibs
  luar_jit.c         jit.* control surface
  luar_bit.c         LuaJIT-compatible 32-bit bit library
  luar_ffi*.c        FFI (in progress)
vendor/lua-5.5.1/    pristine upstream source, the sync reference
vendor/luajit/       the original LuaJIT 2.1 tree (reference + DynASM)
tests/               semantics, compatibility, FFI and benchmark suites
doc/                 architecture, roadmap, compatibility and upstream policy
tools/               upstream sync/diff machinery
```

## Documentation

- [doc/architecture.md](doc/architecture.md) — value model, tiers, FFI and
  GC integration, why the Lua 5.5 core is the semantic authority.
- [doc/roadmap.md](doc/roadmap.md) — milestones 0.1 → 1.0 with exit criteria.
- [doc/compatibility.md](doc/compatibility.md) — the compatibility contract
  and the LuaJIT API matrix.
- [doc/upstream.md](doc/upstream.md) — how LuaR tracks Lua releases.

## License

LuaR is MIT-licensed. It contains code from Lua 5.5.1
(Copyright © 1994–2026 Lua.org, PUC-Rio) and from LuaJIT 2.1
(Copyright © 2005–2026 Mike Pall). See `COPYRIGHT`.
