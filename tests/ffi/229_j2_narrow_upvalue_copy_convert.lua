-- ffi/229_j2_narrow_upvalue_copy_convert.lua -- J2 cross-width copies through
-- an upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/229_j2_narrow_upvalue_copy_convert: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; uint32_t ua; } NUCV;
]]

local function run (name, setup, f, get, want)
  local U = ffi.new("NUCV")
  U.a, U.b, U.c, U.ua = 1, 2, 3, 4
  setup(U)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(U, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(U), want, name .. ": value")
end

run("int8->int32", function (u) u.c = -3 end, function (u, n)
  for _ = 1, n do u.a = u.c end
end, function (u) return u.a end, -3)

run("int16->uint32", function (u) u.b = -3 end, function (u, n)
  for _ = 1, n do u.ua = u.b end
end, function (u) return u.ua end, 4294967293)

run("int32->int16", function (u) u.a = 0x12345 end, function (u, n)
  for _ = 1, n do u.b = u.a end
end, function (u) return u.b end, 0x2345)

run("int32->int8", function (u) u.a = 1000 end, function (u, n)
  for _ = 1, n do u.c = u.a end
end, function (u) return u.c end, -24)

run("uint32->int32", function (u) u.ua = 0xffffffff end, function (u, n)
  for _ = 1, n do u.a = u.ua end
end, function (u) return u.a end, -1)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/229_j2_narrow_upvalue_copy_convert ok")
