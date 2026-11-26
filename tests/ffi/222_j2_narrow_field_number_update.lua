-- ffi/222_j2_narrow_field_number_update.lua -- J2 narrow-field updates from
-- Lua integer scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/222_j2_narrow_field_number_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NNU;
]]

local function run (name, value, want, f, get)
  local s = ffi.new("NNU")
  s.a, s.b, s.c, s.d = 7, 7, 7, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": int64 sibling")
end

run("int32 add", 3, 607, function (o, n)
  local v = 3
  for _ = 1, n do o.a = o.a + v end
end, function (s) return s.a end)

run("int16 sub", 3, -593, function (o, n)
  local v = 3
  for _ = 1, n do o.b = o.b - v end
end, function (s) return s.b end)

run("int8 mul", 3, 103, function (o, n)
  local v = 3
  for _ = 1, n do o.c = o.c * v end
end, function (s) return s.c end)

run("int32 band", 0x55, 5, function (o, n)
  local v = 0x55
  for _ = 1, n do o.a = o.a & v end
end, function (s) return s.a end)

run("int16 shl", 2, 0, function (o, n)
  local v = 2
  for _ = 1, n do o.b = o.b << v end
end, function (s) return s.b end)

run("int8 shl", 2, 0, function (o, n)
  local v = 2
  for _ = 1, n do o.c = o.c << v end
end, function (s) return s.c end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/222_j2_narrow_field_number_update ok")
