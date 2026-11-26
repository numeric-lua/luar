-- ffi/228_j2_narrow_upvalue_field_scalar_divmod.lua -- J2 narrow-field
-- divide/modulo on an upvalue struct from cdata scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/228_j2_narrow_upvalue_field_scalar_divmod: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; uint32_t ua; } NUD;
]]

local function run (name, scalar, value, f, get)
  local v = ffi.new(scalar, value)
  local w = ffi.new("NUD")
  w.a, w.b, w.c, w.ua = 1000000, 1000000, 100, 1000000
  jit.off()
  f(w, v, 200)
  local want = get(w)

  local U = ffi.new("NUD")
  U.a, U.b, U.c, U.ua = 1000000, 1000000, 100, 1000000
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(U, v, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(U), want, name .. ": value")
end

run("int32 div", "int32_t", 3, function (u, v, n)
  for _ = 1, n do u.a = u.a // v end
end, function (u) return u.a end)

run("int32 mod", "int32_t", 3, function (u, v, n)
  for _ = 1, n do u.a = u.a % v end
end, function (u) return u.a end)

run("int16 div", "int16_t", 3, function (u, v, n)
  for _ = 1, n do u.b = u.b // v end
end, function (u) return u.b end)

run("int8 div", "int8_t", 3, function (u, v, n)
  for _ = 1, n do u.c = u.c // v end
end, function (u) return u.c end)

run("uint32 div", "uint32_t", 3, function (u, v, n)
  for _ = 1, n do u.ua = u.ua // v end
end, function (u) return u.ua end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/228_j2_narrow_upvalue_field_scalar_divmod ok")
