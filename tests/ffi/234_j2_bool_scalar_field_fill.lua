-- ffi/234_j2_bool_scalar_field_fill.lua -- J2 field fills from cdata _Bool.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/234_j2_bool_scalar_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; int32_t i; int8_t c; int64_t d; } NBS;
]]

local function run (name, value, f, get, want)
  local s = ffi.new("NBS")
  s.b, s.i, s.c, s.d = false, 0, 0, 4
  local v = ffi.new("_Bool", value)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, v, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": sibling")
end

run("bool true", true, function (o, v, n)
  for _ = 1, n do o.b = v end
end, function (s) return s.b end, true)

run("bool false", false, function (o, v, n)
  for _ = 1, n do o.b = v end
end, function (s) return s.b end, false)

run("int32 true", true, function (o, v, n)
  for _ = 1, n do o.i = v end
end, function (s) return s.i end, 1)

run("int8 false", false, function (o, v, n)
  for _ = 1, n do o.c = v end
end, function (s) return s.c end, 0)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/234_j2_bool_scalar_field_fill ok")
