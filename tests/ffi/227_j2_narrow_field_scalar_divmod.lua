-- ffi/227_j2_narrow_field_scalar_divmod.lua -- J2 narrow-field divide/modulo
-- with cdata scalar operands.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/227_j2_narrow_field_scalar_divmod: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; uint32_t ua; } NSD;
]]

local function run (name, scalar, value, f, get)
  local w = ffi.new("NSD")
  w.a, w.b, w.c, w.ua = 1000000, 1000000, 100, 1000000
  local v = ffi.new(scalar, value)
  jit.off()
  f(w, v, 200)
  local want = get(w)

  local s = ffi.new("NSD")
  s.a, s.b, s.c, s.ua = 1000000, 1000000, 100, 1000000
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, v, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
end

run("int32 div", "int32_t", 3, function (o, v, n)
  for _ = 1, n do o.a = o.a // v end
end, function (s) return s.a end)

run("int32 mod", "int32_t", 3, function (o, v, n)
  for _ = 1, n do o.a = o.a % v end
end, function (s) return s.a end)

run("int16 div", "int16_t", 3, function (o, v, n)
  for _ = 1, n do o.b = o.b // v end
end, function (s) return s.b end)

run("int8 div", "int8_t", 3, function (o, v, n)
  for _ = 1, n do o.c = o.c // v end
end, function (s) return s.c end)

run("uint32 div", "uint32_t", 3, function (o, v, n)
  for _ = 1, n do o.ua = o.ua // v end
end, function (s) return s.ua end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/227_j2_narrow_field_scalar_divmod ok")
