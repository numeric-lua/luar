# Compatibility contract

This document is the promise LuaR makes: a complete Lua 5.5 runtime, a
small additive extension surface, and no legacy Lua versions at all.

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
stock `lua` 5.5.1, with `tests/diff/` (identical output under both
interpreters), and with the official Lua test suite (`tests/upstream/run.sh`).

Intentional, documented differences:

* `luar -v` prints the LuaR banner including the Lua base version.
* The program name defaults to `luar`; `luarc` replaces `luac`.
* `luaL_openlibs` (and the stand-alone interpreter) also open the LuaR
  extensions: `luar`, `jit`, `table.new`/`table.clear`, and `ffi` when
  built.  A host compiled against the **stock** Lua 5.5 headers still expands
  `luaL_openlibs` to `luaL_openselectedlibs(L, ~0, 0)` and therefore gets
  exactly the standard libraries; it can call `luar_openlibs(L)` explicitly
  to opt in.
* Default module paths include `luar/5.5` directories before the Lua ones.
* `LUAR_INIT` is consulted before `LUA_INIT`.
* `LUA_ROOT` can be overridden at build time with `PREFIX`.
* `_VERSION` stays `"Lua 5.5"`; LuaR's own version is in `luar.version`.

## 2. Lua 5.5 only

There is no compatibility mode for an older Lua line.  The practical guide
is `doc/lua55-only.md`.  The build contains no
`LUA_COMPAT_*` switches, and the following are deliberately absent:

* `global` used as an identifier: it is always a reserved word.
* Deprecated math functions (`math.pow`, `math.atan2`, `math.log10`,
  `math.sinh`, ...).
* Legacy C API macros (`lua_strlen`, `lua_objlen`, `lua_equal`,
  `lua_lessthan`, `luaL_checkint`, `luaL_checklong`, ...).
* The LuaJIT 5.1-era `bit` library.  Lua 5.5 has native `&`, `|`, `~`, `<<`
  and `>>`.
* `unpack`, `module`, `setfenv`/`getfenv`, `newproxy`, `string.gfind`,
  `table.maxn`, `math.mod`, `loadstring` and the 5.1 vararg `arg` table.

Code that needs those must be written for Lua 5.5.  LuaR does not ship,
link, or emulate an older Lua runtime.

## 3. LuaJIT-style extension surface

LuaJIT compatibility below means API shape and observable semantics for
portable code.  It does not mean LuaJIT's C API, bytecode, or old-language
semantics.

### 3.1 `table` — implemented

`table.new(narr, nrec)` and `table.clear(t)`. `table.create` is the upstream
Lua 5.5 spelling and is left untouched.

### 3.2 `jit` — control surface only

| API | Status | Notes |
| --- | --- | --- |
| `jit.version`, `jit.version_num` | implemented | LuaR version, not LuaJIT's |
| `jit.os`, `jit.arch` | implemented | `Linux`/`OSX`/..., `x64`/`arm64`/... |
| `jit.status()` | implemented | `true, arch` when the J2 x86-64 engine is active; `false` after `jit.off()` or on unsupported hosts |
| `jit.on()`, `jit.off()` | implemented | toggles native loop entry; the interpreter stays authoritative |
| `jit.flush()` | implemented | bumps the epoch, frees code and clears trace/stats; already-hot Protos are not walked (J1 limitation) |
| `jit.dump(mode)` | implemented | `"t"` prints bytecode snapshots and `"i"` linear IR to stderr; no file/function output argument |
| `jit.opt.start(...)` | implemented | options are parsed/stored; `table=1` toggles the experimental table tier |
| `jit.attach()` | not implemented | planned with the profiler |
| `jit.security()` | not implemented | out of scope |
| `jit.util.*` | not implemented | LuaJIT-specific bytecode introspection |
| `jit.profile` | not implemented | candidate for 0.4 |
| `jit.stats()`, `jit.trace()`, `jit.traces()`, `jit.ir()` | LuaR extension | J1 recorder/J2 introspection (`compiled`, `mcode`, `traces`) |

### 3.3 `ffi` — core implemented (opt-in)

Build with `WITH_FFI=1`; calls are made through libffi, loaded with
`dlopen`.  The full reference is `doc/ffi.md`.

Implemented: `cdef` (typedefs, structs/unions including anonymous and nested
members, enums and their constants, pointers, fixed and variable-length
arrays, function types, extern variables),
`typeof`/`istype`/`sizeof`/`alignof`/`offsetof`, `new` (scalars, arrays,
VLAs, structs/unions; table, string and positional initializers), `cast`,
`string`, `copy`, `fill`, `gc` (Lua or C finalizer), `metatype`, `C`,
`load`, `errno`, `abi`, `os`, `arch`, `NULL`, field/array access, pointer
arithmetic, int64/uint64 wrap-around, comparisons with numbers/nil,
struct-by-value arguments and returns, and variadic calls with documented
default argument promotions.

Deliberate deviations, all documented:

* cdata is full userdata; loading the FFI wraps the global `type()` and
  `tonumber()` so `type(cdata) == "cdata"` and numeric cdata convert.
* A small, gated `/* LUAR */` hook in `src/lvm.c` lets the VM call `__eq`
  for cdata↔number/nil; stock Lua 5.5 never does.  It is compiled into both
  builds (`LUAR_FFI_HOOKS`) but inert without the reserved cdata marker.
* `__thread`/`_Thread_local` globals are readable and writable through
  `ffi.C`/`ffi.load` for the current thread.
* `string.format` accepts integer cdata for integer conversions and uses
  `tostring` for `%s`/`%q`; float cdata is rejected by numeric conversions.
* Structs with fixed array members and unions of any size up to 16 bytes
  (per-eightbyte SSE/INTEGER classification, alignment-sized fields for odd
  sizes) or larger memory-class unions can be passed and returned by value.
* `__attribute__((packed))`/`aligned(n)` structs/unions and `#pragma pack`
  control layout; packed aggregates fail clearly by value.
* Struct and union bitfields with integer/enum/bool bases are supported,
  including mixed base types, union overlap at offset zero and by-value
  passing/returning.
* `ffi.callback` and `ffi.callback_error` implement C→Lua closures through
  libffi, including `cb:set`/`cb:free`; errors in the Lua callback become a
  zero C return plus a stored message.
* Bounds checks for owned arrays are stricter than LuaJIT's.

Not supported: `#` stringification, angle-bracket `#include`,
non-pack `#pragma`,
oversized unions, `long double`/complex/vector
types, C→Lua callbacks `ffi.typeinfo`, and aggregate/variadic
callback shapes.  `ffi.callback` itself is implemented, including
struct-by-value arguments and returns.

### 3.4 Other LuaJIT libraries

| API | Status |
| --- | --- |
| `string.buffer` | candidate (0.7), not required for compatibility |
| `table.clone`, OpenResty `table.*` | not provided (use a Lua module) |
| `LUAJIT_VERSION`, `luaJIT_setmode`, LuaJIT headers | not provided |

## 4. Versioning

* `luar.version` / `LUAR_VERSION` follow semver for the runtime and its
  extension API.
* `luar.lua_version` / `LUAR_LUA_VERSION` name the exact Lua base.
* Bytecode compatibility follows Lua: it changes when Lua changes it.
* Extensions may add functions in minor releases; removal requires a major
  release and a deprecation cycle in `luar.features`.

## 5. What we will not promise

* Binary or source compatibility with LuaJIT's C API, headers or bytecode.
* Running unmodified LuaJIT C modules (they use LuaJIT's C API).
* Any legacy Lua language version.
* Identical performance to LuaJIT before the JIT milestones land. 0.1 is an
  interpreter at stock Lua 5.5 speed; `bench/` reports the gap honestly.
