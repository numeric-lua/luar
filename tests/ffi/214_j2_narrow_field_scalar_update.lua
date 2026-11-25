-- ffi/214_j2_narrow_field_scalar_update.lua -- J2 narrow-field updates from
-- cdata scalar operands.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/214_j2_narrow_field_scalar_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NSU;
]]

local function run (name, scalar, value, f, get)
  local w = ffi.new("NSU")
  w.a, w.b, w.c, w.d = 7, 7, 7, 4
  local v = ffi.new(scalar, value)
  jit.off()
  f(w, v, 200)
  local want = get(w)

  local s = ffi.new("NSU")
  s.a, s.b, s.c, s.d = 7, 7, 7, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, v, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": int64 sibling")
end

run("int32 add", "int32_t", 3, function (o, v, n)
  for _ = 1, n do o.a = o.a + v end
end, function (s) return s.a end)

run("int16 sub", "int16_t", 3, function (o, v, n)
  for _ = 1, n do o.b = o.b - v end
end, function (s) return s.b end)

run("int8 mul", "int8_t", 3, function (o, v, n)
  for _ = 1, n do o.c = o.c * v end
end, function (s) return s.c end)

run("int32 band", "int32_t", 0x55, function (o, v, n)
  for _ = 1, n do o.a = o.a & v end
end, function (s) return s.a end)

run("int16 shl", "int16_t", 2, function (o, v, n)
  for _ = 1, n do o.b = o.b << v end
end, function (s) return s.b end)

run("int8 shl", "int8_t", 2, function (o, v, n)
  for _ = 1, n do o.c = o.c << v end
end, function (s) return s.c end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/214_j2_narrow_field_scalar_update ok")
