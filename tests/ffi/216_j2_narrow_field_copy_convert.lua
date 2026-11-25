-- ffi/216_j2_narrow_field_copy_convert.lua -- J2 cross-width narrow copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/216_j2_narrow_field_copy_convert: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NCC;
]]

local function run (name, setup, f, get, want)
  local s = ffi.new("NCC")
  s.a, s.b, s.c, s.d = 1, 2, 3, 4
  setup(s)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": int64 sibling")
end

run("int16->int32", function (s) s.b = -3 end, function (o, n)
  for _ = 1, n do o.a = o.b end
end, function (s) return s.a end, -3)

run("int8->int16", function (s) s.c = 5 end, function (o, n)
  for _ = 1, n do o.b = o.c end
end, function (s) return s.b end, 5)

run("int16->int8", function (s) s.b = 0x1234 end, function (o, n)
  for _ = 1, n do o.c = o.b end
end, function (s) return s.c end, 0x34)

run("int32->int8", function (s) s.a = 100000 end, function (o, n)
  for _ = 1, n do o.c = o.a end
end, function (s) return s.c end, -96)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/216_j2_narrow_field_copy_convert ok")
