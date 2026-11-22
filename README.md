# Luar

**A Lua 5.5-only runtime with a LuaJIT-style FFI and a tiered JIT path.**

Luar starts from the official Lua 5.5.1 sources and never looks backwards:
no 5.1, 5.2, 5.3 or 5.4 compatibility switches, no legacy APIs, no second
dialect. On top of that exact 5.5 language and C ABI it adds a compact FFI,
the `jit` control surface, `table.new`/`table.clear`, and a concrete,
documented route to a tracing JIT.

The defining rule: **Luar follows Lua.** When Lua 5.6 ships, Luar adopts
it. Each release supports one Lua line, the current one.

```
$ luar -v
Luar 0.186.0 (Lua 5.5.1)  Copyright (C) 2026 Luar contributors
```

## Status (0.186.0)

| Area | State |
| --- | --- |
| Lua 5.5.1 language, stdlib, bytecode | complete; bytecode interoperates with `lua`/`luac` 5.5.1 |
| C API and C module ABI | compatible; a stock `lua5.5` module loads unchanged |
| `luar`, `luarc`, `libluar.a`, `libluar.so`, install | working (`make`, `make shared`, `make install`, `luar.pc`, man page) |
| `luar` identity, `LUAR_INIT`, `luar/5.5` module paths | working |
| `table.new` / `table.clear` | working |
| `jit` control surface | working: `on/off/flush/dump/status/stats/trace/traces/ir/opt`; `status()` is true on x86-64 with J2 |
| FFI | LuaJIT-compatible core: cdef macros/includes, cdata, struct/union bitfields, packed/aligned layouts, packed integer calls/returns and packed-argument callbacks on x86-64, unions to 8 MiB by value, varargs, callbacks + `cb:set/free`, TLS, real-consumer test (`WITH_FFI=1`) |
| native JIT | J1 recorder + J2/J3 x86-64 numeric/table loops, guarded exits, call inlining and experimental generic-table/partial-call tiers. J4 tracing IR (`ir=1`): integer/float table roots, K-ops, multi-table bodies, interior array→hash side traces with snapshot/restore, CALL exits, depth/cycle accounting, guard-exit side linking, and interpreter safepoint yields (`jit.status()` true; ~9x `int_sum`, ~5.5x `float_sum`, ~1.6x `float_div`) |
| upstream tracking | `vendor/` + baseline checker + official-suite runner |

Luar 0.7 is a drop-in replacement for the `lua` 5.5 interpreter and for
`liblua`: same bytecode format, C API and module ABI. Outside the J2
whitelist it runs the stock Lua 5.5 VM (within ~1% of stock in `bench/`).
Compiled numeric for-loops are already ~9x (`int_sum`, ahead of LuaJIT)
and ~4x (`float_loop`) faster; closing the rest of the gap is the job of the later
JIT milestones, not a claim of 0.7.

## Following Lua

`doc/lua56-readiness.md` is the adoption checklist for the next upstream
line; Luar does not add compatibility modes for older ones.

## 5.5 only, by construction

* `global` is always a reserved word; `local global = 1` is a syntax error.
* No `LUA_COMPAT_*` switches exist in the build: deprecated math functions
  (`math.pow`, `math.atan2`, `math.log10`, ...) and legacy API macros
  (`lua_strlen`, `lua_equal`, `luaL_checkint`, ...) are gone.
* No `unpack`, `module`, `setfenv`, `newproxy`, `loadstring` or `bit`
  compatibility library. Use `table.unpack`, `load` and native 5.5 bitwise
  operators (`&`, `|`, `~`, `<<`, `>>`).
* The vendored LuaJIT tree is not part of the repository; only its DynASM
  code generator was kept, ported to native 5.5 operators.

## Build

```sh
make                    # src/luar, src/luarc, src/libluar.a
make shared             # + src/libluar.so.0.186.0 (SONAME libluar.so.0)
make WITH_FFI=1         # + FFI (needs the libffi headers)
make test               # smoke, CLI/ABI, semantics, compat, differential
make test-shared        # embedding + C module against libluar.so
make WITH_FFI=1 test    # additionally runs tests/ffi
make install            # PREFIX=/usr/local, DESTDIR supported
```

Requirements: a C99 compiler, `make`, POSIX.  macOS builds also need
readline headers (`brew install readline`); FFI builds additionally need the
libffi headers (`pkg-config --cflags libffi`).  libffi is loaded with
`dlopen` at run time, so nothing links against it. `make` rebuilds
automatically when `PREFIX` or `WITH_FFI` changes. The shared library is
compiled from separate `.pic.o` objects, so `make` and `make shared` never
rebuild or disturb each other's objects; `make clean` removes both sets.

### Embedding and pkg-config

`make install` installs `libluar.a`, `libluar.so.0.186.0` and the
`libluar.so.0`/`libluar.so` symlinks in `$PREFIX/lib`, together with
`luar.pc`. Both link modes use the same package name:

```sh
cc host.c $(pkg-config --cflags --libs luar) -o host            # shared
cc host.c $(pkg-config --cflags --static --libs luar) -o host   # static
```

`libluar.so` carries the SONAME `libluar.so.0` and exports exactly the Lua
5.5 C API (`lua_*`, `luaL_*`, `luaopen_*`) plus the `luar_*` additions; the
~200 internal `luaX_*` helpers are hidden by `etc/libluar.map`. The SONAME is
Luar's own rather than `liblua.so`: the ABI is Lua 5.5's, but the library may
grow extension symbols, and its name must not collide with a system `liblua`
in the same process. Stock Lua 5.5 C modules load unchanged — when a host
links `libluar.so`, a module built against stock `lua5.5` still binds its
`lua_*` symbols to Luar (`test-shared` checks this with `LD_DEBUG` on glibc).

Static hosts that load C modules at run time must export their own symbols
(`-Wl,-E` on ELF); the `luar`/`luarc` binaries already do. A host linked
against `libluar.so` needs no extra linker flag.

## Try it

```sh
$ luar -v
Luar 0.186.0 (Lua 5.5.1)  Copyright (C) 2026 Luar contributors

$ luar -e 'print(0xdeadbeef & 0xffff, table.new and "table.new" or "?")'
48879	table.new

$ luar -b prog.lua prog.luac   # LuaJIT-compatible bytecode mode
$ luar -joff prog.lua          # LuaJIT-compatible JIT control

$ luar -e 'print(luar.version, luar.lua_version, jit.status())'
0.186.0	Lua 5.5.1	true
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

Lua 5.5.1 is the semantic authority. The LuaJIT-style surface is additive
and the JIT is a new tier over 5.5 bytecode; there is no legacy VM to
graft onto. That keeps 5.5 semantics exact, keeps the C ABI intact, and
makes upstream adoption a merge instead of a language port. The reasoning,
value model and layer diagram are in `doc/architecture.md`; the decision
record is `doc/adr/0001-lua55-core.md`.

```
src/                 Luar engine (derived from Lua 5.5.1, small tagged deltas)
  luar_lib.c         identity, table extensions, extended openlibs
  luar_jit.c         jit.* control surface
  luar_ffi*.c        FFI: type system, cdef parser, cdata, libffi calls
vendor/lua-5.5.1/    pristine upstream snapshot, the sync reference
dynasm/              DynASM code generator, ported to native Lua 5.5 ops
tests/               semantics, differential, compat, FFI, CLI/ABI, upstream
bench/               16-benchmark corpus + cross-interpreter harness
doc/                 architecture, roadmap, compatibility, JIT design, upstream
tools/               upstream drift checker and release sync tool
experimental/        DynASM bytecode-to-native prototype (not in the build)
```

## Tests and benchmarks

- `tests/semantics/` also passes under stock `lua` 5.5.1; `tests/diff/` runs
  identical scripts under both and compares stdout and exit status.
- `tests/compat/strict55.lua` pins the 5.5-only rules: reserved `global`,
  no `bit`, no deprecated math, no legacy library names.
- `tests/consumer/luatorch.lua` drives the real `libluatorch.so` C ABI
  through `ffi.load` (tensor creation, `lt_add`, scalar reads, data copy).
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
- [doc/lua55-only.md](doc/lua55-only.md) — what "5.5 only" means: reserved
  words, native bitwise operators, removed legacy names, strict C API.
- [doc/compatibility.md](doc/compatibility.md) — the 5.5-only contract and
  the LuaJIT API matrix.
- [doc/jit-design.md](doc/jit-design.md) — tracing-JIT design: opcode map,
  IR, guards, GC safepoints, codegen and J1–J5 milestones.
- [doc/ffi.md](doc/ffi.md) — FFI scope, semantics and known limitations.
- [doc/upstream.md](doc/upstream.md) — how Luar tracks Lua, including the
  Lua 5.6 adoption checklist.
- [doc/performance.md](doc/performance.md) — measured baseline and honest
  interpretation.
- [doc/testing.md](doc/testing.md) — suites, differential testing, sanitizers.
- [CHANGELOG.md](CHANGELOG.md) — what 0.186.0 contains.

## License

Luar is MIT-licensed. It contains code from Lua 5.5.1
(Copyright © 1994–2026 Lua.org, PUC-Rio). The DynASM code generator is from
LuaJIT (Copyright © 2005–2026 Mike Pall). See `COPYRIGHT`.
