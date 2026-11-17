# Compatibility contract

This document is the promise LuaR makes to users. It is split into what is
guaranteed, what is emulated for LuaJIT code, and what is deliberately absent.

## 1. The guarantee: Lua 5.5.1

LuaR 0.1 is a drop-in replacement for the official Lua 5.5.1 runtime:

| Surface | Guarantee |
| --- | --- |
| Language syntax and semantics | identical to Lua 5.5.1 |
| Standard library | identical, including error messages where tests rely on them |
| Bytecode | same format and version byte; `luarc` output runs on `lua` 5.5.1 and vice versa |
| C API (`lua.h`, `lauxlib.h`, `lualib.h`) | source- and ABI-compatible |
| C modules | modules compiled against Lua 5.5.1 load and run unchanged |
| Coroutines, GC modes, `debug`, `utf8`, `string.pack` | untouched upstream implementation |

The repository enforces this with `tests/semantics/`, which is also run under
stock `lua` 5.5.1, and with the official Lua test suite
(`tests/upstream/run.sh`).

Intentional, documented differences:

* `luar -v` prints the LuaR banner including the Lua base version.
* The program name defaults to `luar`; `luarc` replaces `luac`.
* `luaL_openlibs` (and the stand-alone interpreter) also open the LuaR
  extensions: `luar`, `jit`, `bit`, and `ffi` when built.
* Default module paths include `luar/5.5` directories before the Lua ones.
* `LUAR_INIT` is consulted before `LUA_INIT`.
* `LUA_ROOT` can be overridden at build time with `PREFIX`.
* `_VERSION` stays `"Lua 5.5"`; LuaR's own version is in `luar.version`.

## 2. LuaJIT compatibility surface

LuaJIT compatibility means API shape and observable semantics for portable
code. It does not mean bug-for-bug behaviour, 5.1 semantics, or binary
compatibility with LuaJIT.

### 2.1 `bit` — implemented

All of `bit.tobit`, `bnot`, `band`, `bor`, `bxor`, `lshift`, `rshift`,
`arshift`, `rol`, `ror`, `bswap`, `tohex`, plus `bit.nbits == 32`.
Operands follow LuaJIT's 32-bit folding rules; shift counts use 5 bits;
results are signed 32-bit integers. Native 5.5 operators (`&`, `|`, `~`,
`<<`, `>>`) are 64-bit and are the recommended form for new code.

### 2.2 `table` — implemented

`table.new(narr, nrec)` and `table.clear(t)`. `table.create` is the upstream
Lua 5.5 spelling and is left untouched.

### 2.3 `jit` — control surface only

| API | Status | Notes |
| --- | --- | --- |
| `jit.version`, `jit.version_num` | implemented | LuaR version, not LuaJIT's |
| `jit.os`, `jit.arch` | implemented | `Linux`/`OSX`/..., `x64`/`arm64`/... |
| `jit.status()` | implemented | truthfully `false` until codegen ships |
| `jit.on()`, `jit.off()` | no-ops | accepted; enable/disable later |
| `jit.flush()` | no-op | clears nothing yet |
| `jit.opt.start(...)` | implemented | options parsed and stored; thresholds used later |
| `jit.attach()` | not implemented | planned with the profiler |
| `jit.security()` | not implemented | out of scope |
| `jit.util.*` | not implemented | bytecode is 5.5, LuaJIT's util is 5.1-shaped |
| `jit.profile` | not implemented | candidate for 0.4 |

### 2.4 `ffi` — milestone 0.2

See `doc/ffi.md` once the implementation lands. Planned subset: `cdef`,
`typeof`, `istype`, `sizeof`, `alignof`, `offsetof`, `new`, `cast`,
`metatype`, `gc`, `string`, `copy`, `fill`, `C`, `load`, `errno`, `abi`,
`os`, `arch`, `NULL`, cdata arithmetic and field access, fixed-argument
calls.

Not planned in 0.2: C preprocessor, bitfields, varargs calls, callbacks from
C to Lua (`ffi.callback`), `ffi.typeinfo`, complex/vector types.

### 2.5 Other LuaJIT libraries

| API | Status |
| --- | --- |
| `string.buffer` | candidate (0.7), not required for compatibility |
| `table.clone`, OpenResty `table.*` | not provided (use a Lua module) |
| `os.exit` 5.2 semantics | inherited from Lua 5.5 |
| `LUAJIT_VERSION`, `luaJIT_setmode`, LuaJIT headers | not provided |

## 3. Deliberately absent: Lua 5.1

LuaR does not simulate Lua 5.1. Code and modules that require it must be
ported:

* no `unpack` (use `table.unpack`), `module`, `setfenv`/`getfenv`,
  `newproxy`, `string.gfind`, `table.maxn`, `math.mod`, `loadstring`
* no `arg` table in vararg functions (use `...`; the CLI `arg` table exists)
* no 5.1 `__len`/`__ipairs` edge semantics; 5.5 rules apply
* no 5.1 C API (`lua_open`, `luaL_getn`, `LUA_REGISTRYINDEX` layout changes)

LuaJIT's `ffi` was the reason most numeric Lua code stayed on 5.1; that is
what milestone 0.2 addresses.

## 4. Lua 5.5 language changes that bite when porting from LuaJIT

* **Integers exist.** `1` and `1.0` differ; `/` always produces a float;
  `//` is floor division; integers wrap on overflow; `math.type` matters.
* **`global` is a keyword** and introduces scoped strict-global declarations
  (`global x`, `global<const> *`). Inside such a scope every free name must
  be declared.
* **`for` control variables are read-only.** Assigning to them is a compile
  error; shadow them in the body instead.
* **`__call` chains are limited** to 15 objects.
* **`error(nil)` produces the string `"<no error object>"`.**
* **C API changes:** `lua_newstate` takes a seed argument, `lua_resetthread`
  is deprecated in favour of `lua_closethread`, `lua_setcstacklimit` is
  deprecated, GC parameters moved to `LUA_GCPARAM`, `lua_dump` calls the
  writer one extra time, and `nresults` is capped at 250.
* **Numeric string coercion** is narrower than 5.1 (no implicit coercion in
  concatenation of numbers? it still works, but `"10" + 1` is fine while
  `"0x10" + 0` is hexadecimal-aware; test rather than assume).

## 5. Versioning

* `luar.version` / `LUAR_VERSION` follow semver for the runtime and its
  extension API.
* `luar.lua_version` / `LUAR_LUA_VERSION` name the exact Lua base.
* Bytecode compatibility follows Lua: it changes when Lua changes it.
* Extensions may add functions in minor releases; removal requires a major
  release and a deprecation cycle in `luar.features`.

## 6. What we will not promise

* Binary compatibility with LuaJIT, its headers or its bytecode.
* Running unmodified LuaJIT C modules (they use LuaJIT's C API).
* Identical performance to LuaJIT before the JIT milestones land. 0.1 is an
  interpreter at stock Lua 5.5 speed; `bench/` reports the gap honestly.
