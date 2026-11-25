-- ffi/219_j2_narrow_upvalue_field_scalar_update.lua -- J2 narrow-field
-- updates on an upvalue struct from cdata scalar operands.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/219_j2_narrow_upvalue_field_scalar_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NUS;
]]

local function run (name, scalar, value, expected, f, get)
  local U = ffi.new("NUS")
  U.a, U.b, U.c, U.d = 7, 7, 7, 4
  local v = ffi.new(scalar, value)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(U, v, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(U), expected, name .. ": field")
  H.eq(U.d, 4, name .. ": int64 sibling")
end

run("int32 add", "int32_t", 3, 607, function (u, v, n)
  for _ = 1, n do u.a = u.a + v end
end, function (u) return u.a end)

run("int16 sub", "int16_t", 3, -593, function (u, v, n)
  for _ = 1, n do u.b = u.b - v end
end, function (u) return u.b end)

run("int8 mul", "int8_t", 3, 103, function (u, v, n)
  for _ = 1, n do u.c = u.c * v end
end, function (u) return u.c end)

run("int32 band", "int32_t", 0x55, 5, function (u, v, n)
  for _ = 1, n do u.a = u.a & v end
end, function (u) return u.a end)

run("int16 shl", "int16_t", 2, 0, function (u, v, n)
  for _ = 1, n do u.b = u.b << v end
end, function (u) return u.b end)

run("int8 shl", "int8_t", 2, 0, function (u, v, n)
  for _ = 1, n do u.c = u.c << v end
end, function (u) return u.c end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/219_j2_narrow_upvalue_field_scalar_update ok")
