-- ffi/230_j2_narrow_field_number_divmod.lua -- J2 narrow-field divide/modulo
-- from Lua integer scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/230_j2_narrow_field_number_divmod: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; uint32_t ua; } NND;
]]

local function run (name, value, f, get)
  local w = ffi.new("NND")
  w.a, w.b, w.c, w.ua = 1000000, 1000000, 100, 1000000
  jit.off()
  f(w, value, 200)
  local want = get(w)

  local s = ffi.new("NND")
  s.a, s.b, s.c, s.ua = 1000000, 1000000, 100, 1000000
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, value, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
end

run("int32 div", 3, function (o, v, n)
  for _ = 1, n do o.a = o.a // v end
end, function (s) return s.a end)

run("int32 mod", 3, function (o, v, n)
  for _ = 1, n do o.a = o.a % v end
end, function (s) return s.a end)

run("int16 div", 3, function (o, v, n)
  for _ = 1, n do o.b = o.b // v end
end, function (s) return s.b end)

run("int8 div", 3, function (o, v, n)
  for _ = 1, n do o.c = o.c // v end
end, function (s) return s.c end)

run("uint32 div", 3, function (o, v, n)
  for _ = 1, n do o.ua = o.ua // v end
end, function (s) return s.ua end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/230_j2_narrow_field_number_divmod ok")
