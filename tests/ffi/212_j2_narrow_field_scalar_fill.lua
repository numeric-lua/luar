-- ffi/212_j2_narrow_field_scalar_fill.lua -- J2 narrow-field fills from cdata.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/212_j2_narrow_field_scalar_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NSL;
]]

local function run (name, scalar, f, get, value)
  local s = ffi.new("NSL")
  s.a, s.b, s.c, s.d = 1, 2, 3, 4
  local v = ffi.new(scalar, value)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, v, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), value, name .. ": field value")
  H.eq(s.d, 4, name .. ": int64 sibling")
end

run("int32", "int32_t", function (o, v, n)
  for _ = 1, n do o.a = v end
end, function (s) return s.a end, 7)

run("int16", "int16_t", function (o, v, n)
  for _ = 1, n do o.b = v end
end, function (s) return s.b end, -7)

run("int8", "int8_t", function (o, v, n)
  for _ = 1, n do o.c = v end
end, function (s) return s.c end, 7)

-- Sibling independence for a narrow fill.
do
  local s = ffi.new("NSL")
  s.a, s.b, s.c, s.d = 1, 2, 3, 4
  local v = ffi.new("int8_t", -3)
  local function fill (o, x, n) for _ = 1, n do o.c = x end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  fill(s, v, 200)
  H.check(jit.stats().tint_hits > before, "sibling fill did not run")
  H.eq(s.a, 1, "int32 sibling")
  H.eq(s.b, 2, "int16 sibling")
  H.eq(s.c, -3, "int8 value")
  H.eq(s.d, 4, "int64 sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/212_j2_narrow_field_scalar_fill ok")
