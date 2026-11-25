-- ffi/218_j2_narrow_upvalue_field_update.lua -- J2 narrow-field updates on
-- an upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/218_j2_narrow_upvalue_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NUU;
]]

local function run (name, setup, f, get)
  local U = ffi.new("NUU")
  U.a, U.b, U.c, U.d = 7, 7, 7, 4
  setup(U)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(U, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  return U, get
end

do
  local U, get = run("int32 add", function (u) u.a = 7 end, function (u, n)
    for _ = 1, n do u.a = u.a + 3 end
  end, function (u) return u.a end)
  H.eq(U.a, 607, "int32 add")
  H.eq(U.d, 4, "int32 sibling")
end

do
  local U = run("int16 sub", function (u) u.b = 7 end, function (u, n)
    for _ = 1, n do u.b = u.b - 3 end
  end, function (u) return u.b end)
  H.eq(U.b, -593, "int16 sub")
  H.eq(U.d, 4, "int16 sibling")
end

do
  local U = run("int8 mul", function (u) u.c = 7 end, function (u, n)
    for _ = 1, n do u.c = u.c * 3 end
  end, function (u) return u.c end)
  H.eq(U.c, 103, "int8 mul")
  H.eq(U.d, 4, "int8 sibling")
end

do
  local U = run("int32 band", function (u) u.a = 7 end, function (u, n)
    for _ = 1, n do u.a = u.a & 0x55 end
  end, function (u) return u.a end)
  H.eq(U.a, 5, "int32 band")
  H.eq(U.d, 4, "band sibling")
end

do
  local U = run("int16 shl", function (u) u.b = 7 end, function (u, n)
    for _ = 1, n do u.b = u.b << 2 end
  end, function (u) return u.b end)
  H.eq(U.b, 0, "int16 shl")
  H.eq(U.d, 4, "shl sibling")
end

do
  local U = run("int8 shl", function (u) u.c = 7 end, function (u, n)
    for _ = 1, n do u.c = u.c << 2 end
  end, function (u) return u.c end)
  H.eq(U.c, 0, "int8 shl")
  H.eq(U.d, 4, "int8 shl sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/218_j2_narrow_upvalue_field_update ok")
