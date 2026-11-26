-- ffi/221_j2_narrow_field_float_fill.lua -- J2 narrow-field fills from Lua
-- floating scalars (C truncation toward zero).
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/221_j2_narrow_field_float_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NFF;
]]

local function run (name, value, want, f, get)
  local s = ffi.new("NFF")
  s.a, s.b, s.c, s.d = 1, 2, 3, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": int64 sibling")
end

run("int32", 7.9, 7, function (o, n)
  local v = 7.9
  for _ = 1, n do o.a = v end
end, function (s) return s.a end)

run("int16", -3.9, -3, function (o, n)
  local v = -3.9
  for _ = 1, n do o.b = v end
end, function (s) return s.b end)

run("int8", 5.5, 5, function (o, n)
  local v = 5.5
  for _ = 1, n do o.c = v end
end, function (s) return s.c end)

run("int8 negative", -5.5, -5, function (o, n)
  local v = -5.5
  for _ = 1, n do o.c = v end
end, function (s) return s.c end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/221_j2_narrow_field_float_fill ok")
