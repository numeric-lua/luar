# The Lua 5.5-only surface

LuaR implements exactly one language line: the current official Lua, which
is **5.5.1** for this release. There is no compatibility mode, no
`LUA_COMPAT_*` switch, and no legacy API. This document collects what that
means in practice; `doc/compatibility.md` is the formal contract.

## 1. One language, no switches

The build removes the compatibility code that upstream Lua 5.5 still ships
behind compile-time options:

| Upstream option | LuaR |
| --- | --- |
| `LUA_COMPAT_GLOBAL` | removed; `global` is always a reserved word |
| `LUA_COMPAT_MATHLIB` | removed; `math.pow`, `math.atan2`, `math.log10`, `math.sinh`, `math.cosh`, `math.tanh` do not exist |
| `LUA_COMPAT_APIINTCASTS` | removed; `lua_strlen`, `lua_objlen`, `lua_equal`, `lua_lessthan`, `luaL_checkint`, `luaL_checklong`, `luaL_checkunsigned` do not exist |

The upstream compatibility section in `src/luaconf.h` is gone. If a program
needs an older Lua, it must be ported; LuaR will not emulate it.

## 2. Language rules that always apply

* **`global` is a keyword.** `local global = 1` and `global = 1` are syntax
  errors. Declarations are `global name`, `global<const> name`,
  `global function name`, `global *` and `global<const> *`; inside such a
  scope every free name must be declared.
* **`for` control variables are read-only.** Assigning to `i` or `k` in the
  loop body is a compile error; declare a local with the same name to
  shadow it.
* **`__call` chains are limited to 15 objects.**
* **`error(nil)` produces the string `"<no error object>"`.**
* **Integers and floats are distinct.** `1` is an integer, `1.0` is a
  float, `/` always produces a float, `//` floors, integers wrap on
  overflow, and `math.type` reports the subtype.

## 3. Bitwise operators, not a `bit` library

Lua 5.5 has native 64-bit bitwise operators and LuaR ships no `bit` module:

```lua
local a = 0xff
print(a & 0x0f)          --> 15
print(a | 0x100)         --> 511
print(a ~ 0x0f)          --> 240
print(1 << 62)           --> 4611686018427387904
print(-1 >> 63)          --> 1              (logical shift)
print(~0)                --> -1
```

Semantics worth remembering:

* operands must be integers or floats with an exact integer value;
  `1.5 & 1` is an error;
* shift counts use the low six bits conceptually, and negative counts shift
  the other way; absolute counts of 64 or more produce zero;
* results are integers and wrap as 64-bit two's-complement values;
* `__band`, `__bor`, `__bxor`, `__shl`, `__shr` and `__bnot` are the
  metamethods for non-number operands.

`tests/semantics/bitwise.lua` pins all of this against upstream Lua 5.5.1.

## 4. Library names that are gone

These old spellings do not exist anywhere in the runtime:

| Removed | Use instead |
| --- | --- |
| `unpack(t)` | `table.unpack(t)` |
| `loadstring(s)` | `load(s)` |
| `module("m")`, `setfenv`, `getfenv` | `_ENV` / locals; return a table |
| `newproxy` | userdata + metatable, or `<close>` |
| `string.gfind` | `string.gmatch` |
| `table.maxn` | compute it, or `#t` for sequences |
| `math.mod`, `math.pow`, `math.atan2`, `math.log10` | `%`, `^`, `math.atan`, `math.log(x, 10)` |
| `bit.*` | native `&`, `|`, `~`, `<<`, `>>` |
| the 5.1 vararg `arg` table | `...` (`arg` exists only for CLI scripts) |

`table.create(narr, nrec)` is the upstream 5.5 table preallocation function.
`table.new`/`table.clear` remain available as LuaJIT-style extensions; they
are additions, not compatibility shims.

## 5. C API and modules

The C API is the Lua 5.5 API. In particular:

* `lua_newstate` takes a seed argument;
* `lua_resetthread` is deprecated in favour of `lua_closethread`;
* GC parameters use `LUA_GCPARAM`;
* `lua_dump` calls the writer once more to signal the end;
* the maximum number of required results is 250;
* there is no `lua_open`, `luaL_getn`, `lua_strlen`, `lua_equal` or
  `lua_lessthan`.

C modules compiled for Lua 5.5 load unchanged because the ABI is the
upstream one (`tests/cli.sh` proves it). LuaJIT's C API and bytecode are
different and are not supported.

## 6. What LuaR adds, and what it does not

Additions are opt-in extensions layered on the 5.5 language:

* `luar` identity table, `LUAR_INIT`, `luar/5.5` module directories;
* `ffi` (`WITH_FFI=1`), `jit` control surface, `table.new`/`table.clear`;
* `luar -j ...` and `luar -b ...` command-line conveniences.

They do not change 5.5 semantics. In particular `jit.status()` is truthful
about the absence of a native engine, and the FFI is opt-in and documented
in `doc/ffi.md`; neither reintroduces an older Lua language.

## 7. How this is enforced

* `tests/semantics/` is upstream 5.5.1 conformance and must also pass under
  stock `lua` 5.5.1.
* `tests/diff/` compares stdout and exit status between stock `lua` and
  `luar` on the same scripts.
* `tests/compat/strict55.lua` asserts the strict rules above (reserved
  `global`, no `bit`, no deprecated math, no legacy names).
* `tools/upstream-check.sh` keeps the deliberately small `src/` delta
  reviewable, and the official Lua suite runs in `tests/upstream/`.
