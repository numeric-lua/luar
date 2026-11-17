-- compat/luajit_idioms.lua -- code written for LuaJIT must at least probe
-- the environment safely and fall back when a feature is absent.

local H = dofile("tests/helpers.lua")

-- the canonical probe
local ok, ffi = pcall(require, "ffi")
if luar.features.ffi then
  H.check(ok, "ffi is advertised but failed to load: " .. tostring(ffi))
else
  H.check(not ok, "ffi is not built but require succeeded")
end

-- jit.status() pattern
local jit_on = jit and jit.status()
H.eq(jit_on, false)

-- table.new/clear pattern
local cache = table.new(0, 64)
cache["k"] = 1
table.clear(cache)
H.eq(next(cache), nil)

-- LuaJIT-style int32 normalisation with bit
local function i32(x) return bit.tobit(x) end
H.eq(i32(0x7fffffff), 0x7fffffff)
H.eq(i32(0x80000000), -0x80000000)
H.eq(i32(0x100000000), 0)

-- 5.5 native bitwise operators replace most bit.* uses
H.eq(0xff & 0x0f, 15)
H.eq(0x80000000 | 0, 0x80000000)
H.eq(-1 >> 63, 1 << 0)

print("compat/luajit_idioms ok")
