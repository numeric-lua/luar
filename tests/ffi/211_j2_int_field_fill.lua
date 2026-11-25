-- ffi/211_j2_int_field_fill.lua -- J2 fills for narrow integer fields.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/211_j2_int_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NIF;
]]

local function run (name, get, f, value)
  local s = ffi.new("NIF")
  s.a, s.b, s.c, s.d = 1, 2, 3, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), value, name .. ": field value")
end

run("int32", function (s) return s.a end, function (o, n)
  for _ = 1, n do o.a = 7 end
end, 7)
run("int16", function (s) return s.b end, function (o, n)
  for _ = 1, n do o.b = -7 end
end, -7)
run("int8", function (s) return s.c end, function (o, n)
  for _ = 1, n do o.c = 7 end
end, 7)

-- Fields must stay independent.
do
  local s = ffi.new("NIF")
  s.a, s.b, s.c, s.d = 1, 2, 3, 4
  local function fill (o, n) for _ = 1, n do o.c = -3 end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "sibling fill did not run")
  H.eq(s.a, 1, "int32 sibling")
  H.eq(s.b, 2, "int16 sibling")
  H.eq(s.c, -3, "int8 value")
  H.eq(s.d, 4, "int64 sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/211_j2_int_field_fill ok")
