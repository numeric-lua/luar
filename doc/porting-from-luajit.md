# Porting from LuaJIT to LuaR

LuaR targets Lua 5.5 and newer. A LuaJIT codebase is, in practice, a Lua 5.1
codebase with FFI and `bit`. This guide lists what changes, in the order that
tends to break builds.

## 1. Language: 5.1 → 5.5

These are upstream Lua changes, not LuaR choices. Run the code under stock
`lua` 5.5.1 first; if it passes there, it passes in LuaR.

| LuaJIT / 5.1 | Lua 5.5 / LuaR |
| --- | --- |
| `unpack(t)` | `table.unpack(t)` |
| `loadstring(s)` | `load(s)` |
| `module("m")`, `setfenv`, `getfenv` | `_ENV` upvalues; return a table or use locals |
| `newproxy` | userdata with a metatable (`<close>` for finalisation) |
| `string.gfind` | `string.gmatch` |
| `table.maxn` | compute it, or use `#` for sequences |
| `math.mod` | `math.fmod` / `%` |
| `arg` in vararg functions | `...` and `select` |
| `1` is a double | `1` is an integer, `1.0` is a float; `/` always returns a float |
| `tostring(1)` is `"1"` | still `"1"`; `string.format("%d", 1.0)` fails on floats |
| integer overflow is float | integers wrap; use floats if you need saturation |
| `#t` undefined for holes | still undefined for holes; avoid |
| `0x10` integer literal | integer, exact up to 2^63 |
| no `goto` | `goto`/labels are available and fast |
| `error(nil)` keeps nil | becomes the string `"<no error object>"` |
| `__call` chains arbitrary | limited to 15 |
| for-loop variable writable | read-only; shadow it instead |

Lua 5.5 additions worth using: `global` declarations (strict globals in a
scope), `table.create(narr, nrec)`, `<const>`/`<close>`, `math.type`,
`utf8`, `string.pack`.

## 2. `bit`

LuaR ships the full LuaJIT `bit` library with 32-bit semantics, so existing
code runs. New code should prefer the native 64-bit operators (`&`, `|`, `~`,
`<<`, `>>`, `//`), which do not box and are what the JIT will optimise first.

```lua
-- old
local lo = bit.band(x, 0xffff)
-- new (64-bit integer semantics; mask explicitly if you need 32 bits)
local lo = x & 0xffff
```

If the old code relied on `bit.tobit` for wraparound at 2^32, keep using
`bit.tobit` or mask with `& 0xffffffff` and convert sign explicitly.

## 3. FFI

LuaR's FFI is a LuaJIT-compatible subset (milestone 0.2). The porting rules:

* `ffi.cdef` keeps working for typedefs, structs, unions, enums, pointers,
  arrays and function declarations. The C preprocessor is not supported:
  define constants yourself or declare them as enums.
* `ffi.new`, `ffi.cast`, `ffi.sizeof`, `ffi.offsetof`, `ffi.string`,
  `ffi.copy`, `ffi.fill`, `ffi.gc`, `ffi.metatype`, `ffi.C`, `ffi.load`,
  `ffi.errno` are supported.
* **Varargs calls** (`ffi.C.printf(fmt, ...)`) are not in 0.2. Use a fixed
  prototype such as `int snprintf(char *buf, size_t n, const char *fmt, ...)`
  only after varargs support lands; until then call narrower shims.
* **Bitfields** are not parsed. Replace them with explicit integer fields
  and masks.
* **`ffi.callback`** (C calling Lua) is not supported. Prefer a C shim that
  calls back through a registered Lua function only when that lands.
* 64-bit integers are cdata (`1LL` equivalent); arithmetic follows LuaJIT:
  it wraps and stays 64-bit. Convert with `tonumber` when safe.
* Pointer arithmetic works on `T*`; comparisons are pointer comparisons.
* `tostring(cdata)` prints a `cdata<...>` form; do not parse it.

Because the C ABI is Lua 5.5's, you can also replace hot FFI glue with a
small C module built against `luar.pc` — it will load unchanged.

## 4. The `jit` namespace

Do not assume a JIT is present. The portable pattern is:

```lua
local has_jit = jit and jit.status()
if has_jit then
  -- JIT-specific tuning, jit.opt.start, jit.util, ...
end
```

In LuaR 0.1, `jit.status()` returns `false` and `luar.features.jit` is
`false`. `jit.on/off/flush` are accepted no-ops, `jit.opt.start` records
options for the future engine. `jit.util`, `jit.profile`, `jit.attach` are
not available; code that depends on them must degrade gracefully.

Do not use `jit.util.funcinfo` to detect "is this a Lua function?" — use
`debug.getinfo` or `type(f) == "function"` and `debug.getinfo(f, "S").what`.

## 5. C API and binary modules

* LuaR exports the Lua 5.5 C API. A module built with
  `pkg-config --cflags lua5.5` loads and runs (tested in CI).
* LuaJIT's C API extensions (`luaJIT_setmode`, `LUAJIT_VERSION`,
  `luaL_openlibs` differences, `lua_State` layout) do not exist.
* Modules linked against `libluajit-5.1.so` will not load. Rebuild them
  against Lua 5.5 headers; the source usually needs only version guards.
* Lua 5.1-only APIs (`lua_open`, `luaL_getn`, ...) are gone.

## 6. Performance expectations

LuaR 0.1 is the stock Lua 5.5 interpreter plus extensions. Expect:

* parity with `lua` 5.5.1 on interpreter workloads (same core);
* slower than LuaJIT 2.1 on numeric loops until the JIT milestones land;
* FFI call overhead higher than LuaJIT's direct stubs (libffi), acceptable
  for coarse-grained calls, visible in chatty bindings.

Measure before rewriting: `bench/` has the same corpus for `lua`, `luajit`
and `luar`. Optimise the algorithms and call granularity first; the JIT is
coming, and it will reward integer/float loops and table access first.

## 7. A practical porting checklist

1. Run the test suite under `lua` 5.5.1. Fix 5.1-isms until green.
2. Run it under `luar`. It must behave identically; report deviations.
3. Replace `unpack`, `loadstring`, `module`, `setfenv`, `string.gfind`.
4. Audit number handling: where do integers become floats? Add
   `math.type` assertions in tests around bit-twiddling and indices.
5. Audit `#` usage on tables with holes; make sequences explicit.
6. Port FFI declarations; remove macros/bitfields or wrap them in C.
7. Guard `jit.*` usage behind `jit.status()`.
8. Benchmark the hot paths against the LuaJIT baseline and record the gap.
9. Keep a compatibility shim for 5.1 spellings if the code must also run on
   LuaJIT during the transition:

```lua
local unpack = table.unpack or unpack
local loadstring = loadstring or load
local bit = bit or require("bit")
```

This shim is the only supported way to keep one codebase on both runtimes;
LuaR itself will not grow 5.1 APIs.
