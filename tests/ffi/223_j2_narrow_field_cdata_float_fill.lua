-- ffi/223_j2_narrow_field_cdata_float_fill.lua -- J2 narrow-field fills from
-- cdata floating scalars (C truncation toward zero).
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/223_j2_narrow_field_cdata_float_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NCF;
]]

local function run (name, scalar, value, want, f, get)
  local s = ffi.new("NCF")
  s.a, s.b, s.c, s.d = 1, 2, 3, 4
  local v = ffi.new(scalar, value)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, v, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": int64 sibling")
end

run("int32 double", "double", 7.9, 7, function (o, v, n)
  for _ = 1, n do o.a = v end
end, function (s) return s.a end)

run("int16 float", "float", -3.9, -3, function (o, v, n)
  for _ = 1, n do o.b = v end
end, function (s) return s.b end)

run("int8 double", "double", 5.5, 5, function (o, v, n)
  for _ = 1, n do o.c = v end
end, function (s) return s.c end)

run("int8 negative", "double", -5.5, -5, function (o, v, n)
  for _ = 1, n do o.c = v end
end, function (s) return s.c end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/223_j2_narrow_field_cdata_float_fill ok")
