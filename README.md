# LuaR

**A Lua 5.5-forward runtime with LuaJIT's superpowers — and without
LuaJIT's time freeze.**

LuaJIT stopped at Lua 5.1 and stayed there. LuaR starts from the official
Lua 5.5.1 sources, keeps the complete Lua 5.5 language, C API and module
ABI, and adds the things people actually missed: a compact FFI, the `jit`
control surface, `table.new`/`table.clear`, 32-bit `bit` compatibility, and
a tiered tracing JIT on a concrete, documented path.

The defining rule: **LuaR follows Lua.** When Lua 5.6 ships, LuaR adopts
it. There is no second, frozen dialect to maintain.

```
$ luar -v
LuaR 0.1.0 (Lua 5.5.1)  Copyright (C) 2026 LuaR contributors
```

## Status (0.1.0)

| Area | State |
| --- | --- |
| Lua 5.5.1 language, stdlib, bytecode | complete; bytecode interoperates with `lua`/`luac` 5.5.1 |
| C API and C module ABI | compatible; a stock `lua5.5` module loads unchanged |
| `luar`, `luarc`, `libluar.a`, install | working (`make`, `make install`, `luar.pc`, man page) |
| `luar` identity, `LUAR_INIT`, `luar/5.5` module paths | working |
| `bit` (LuaJIT 32-bit semantics) | complete, value-for-value against LuaJIT 2.1 |
| `table.new` / `table.clear` | working |
| `jit` control surface | working; `jit.status()` truthfully `false` (no native engine yet) |
| FFI | LuaJIT-compatible core behind `WITH_FFI=1` (0.2 preview) |
| native JIT | designed (`doc/jit-design.md`); DynASM proof in `experimental/tracejit/` |
| upstream tracking | `vendor/` + baseline checker + official-suite runner |

LuaR 0.1 is a drop-in replacement for the `lua` 5.5 interpreter and for
`liblua`: same bytecode format, C API and module ABI. Performance is stock
Lua 5.5 performance (same VM): in `bench/`, LuaR is within ~1% of stock Lua
in aggregate, while LuaJIT 2.1 is 6.7× faster with the JIT on. Closing that
gap is the job of the JIT milestones, not a claim of 0.1.

## Build

```sh
make                    # src/luar, src/luarc, src/libluar.a
make WITH_FFI=1         # + FFI (needs the libffi headers)
make test               # smoke, CLI/ABI, semantics, compat, differential
make WITH_FFI=1 test    # additionally runs tests/ffi
make install            # PREFIX=/usr/local, DESTDIR supported
```

Requirements: a C99 compiler, `make`, POSIX. FFI builds additionally need
the libffi headers (`pkg-config --cflags libffi`); libffi is loaded with
`dlopen` at run time, so nothing links against it. `make` rebuilds
automatically when `PREFIX` or `WITH_FFI` changes.

## Try it

```sh
$ luar -v
LuaR 0.1.0 (Lua 5.5.1)  Copyright (C) 2026 LuaR contributors

$ luar -e 'print(bit.tohex(0xdeadbeef), table.new and "table.new" or "?")'
deadbeef	table.new

$ luar -b prog.lua prog.luac   # LuaJIT-compatible bytecode mode
$ luar -joff prog.lua          # LuaJIT-compatible JIT control (no-op today)

$ luar -e 'print(luar.version, luar.lua_version, jit.status())'
0.1.0	Lua 5.5.1	false
```

With `WITH_FFI=1`:

```lua
local ffi = require("ffi")
ffi.cdef[[
  typedef struct { double x, y; } point_t;
  double sqrt(double);
]]
local p = ffi.new("point_t")
p.x, p.y = 3, 4
print(ffi.C.sqrt(p.x * p.x + p.y * p.y))   -- 5.0
```

## How it is built

Lua 5.5.1 is the semantic authority; the LuaJIT surface is additive and the
JIT is a new tier over 5.5 bytecode, not a graft onto LuaJIT's 5.1-shaped
VM. That keeps 5.5 semantics exact, keeps the C ABI intact, and makes
upstream adoption a merge instead of a language port. The reasoning, value
model and layer diagram are in `doc/architecture.md`; the decision record is
`doc/adr/0001-lua55-core.md`.

```
src/                 LuaR engine (derived from Lua 5.5.1, small tagged deltas)
  luar_lib.c         identity, table extensions, extended openlibs
  luar_jit.c         jit.* control surface
  luar_bit.c         32-bit LuaJIT-compatible bit library
  luar_ffi*.c        FFI: type system, cdef parser, cdata, libffi calls
vendor/lua-5.5.1/    pristine upstream snapshot, the sync reference
vendor/luajit/       original LuaJIT 2.1 tree (reference + DynASM)
tests/               semantics, differential, compat, FFI, CLI/ABI, upstream
bench/               16-benchmark corpus + cross-interpreter harness
doc/                 architecture, roadmap, compatibility, JIT design, upstream
tools/               upstream drift checker and release sync tool
experimental/        DynASM bytecode-to-native prototype (not in the build)
```

## Tests and benchmarks

- `tests/semantics/` also passes under stock `lua` 5.5.1; `tests/diff/` runs
  identical scripts under both and compares stdout and exit status.
- The official Lua 5.5.1 test suite passes 31/31 files and the canonical
  `all.lua` (`make test-upstream`).
- `tools/upstream-check.sh` fails CI on any unrecorded drift from
  `vendor/lua-5.5.1`.
- `make bench` runs the microbenchmark corpus; `make bench-compare` produces
  the four-interpreter comparison in `doc/performance.md`.

## Documentation

- [doc/architecture.md](doc/architecture.md) — value model, tiers, FFI/GC
  integration, why Lua 5.5 is the semantic authority.
- [doc/roadmap.md](doc/roadmap.md) — 0.1 → 1.0 milestones with exit criteria.
- [doc/compatibility.md](doc/compatibility.md) — the compatibility contract
  and the LuaJIT API matrix.
- [doc/jit-design.md](doc/jit-design.md) — tracing-JIT design: opcode map,
  IR, guards, GC safepoints, codegen and J1–J5 milestones.
- [doc/ffi.md](doc/ffi.md) — FFI scope, semantics and known limitations.
- [doc/upstream.md](doc/upstream.md) — how LuaR tracks Lua, including the
  Lua 5.6 adoption checklist.
- [doc/performance.md](doc/performance.md) — measured baseline and honest
  interpretation.
- [doc/testing.md](doc/testing.md) — suites, differential testing, sanitizers.
- [doc/porting-from-luajit.md](doc/porting-from-luajit.md) — migration guide
  for LuaJIT codebases.
- [CHANGELOG.md](CHANGELOG.md) — what 0.1.0 contains.

## License

LuaR is MIT-licensed. It contains code from Lua 5.5.1
(Copyright © 1994–2026 Lua.org, PUC-Rio) and from LuaJIT 2.1
(Copyright © 2005–2026 Mike Pall). See `COPYRIGHT`.
