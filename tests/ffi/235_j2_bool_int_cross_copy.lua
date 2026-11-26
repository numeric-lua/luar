-- ffi/235_j2_bool_int_cross_copy.lua -- J2 bool/integer field cross copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/235_j2_bool_int_cross_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; int32_t i; int8_t c; int64_t d; } BIC;
]]

local function run (name, setup, f, get, want)
  local s = ffi.new("BIC")
  s.b, s.i, s.c, s.d = false, 0, 0, 4
  setup(s)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": sibling")
end

run("bool->int32", function (s) s.b = true end, function (o, n)
  for _ = 1, n do o.i = o.b end
end, function (s) return s.i end, 1)

run("bool->int8", function (s) s.b = false end, function (o, n)
  for _ = 1, n do o.c = o.b end
end, function (s) return s.c end, 0)

run("int32->bool true", function (s) s.i = 2 end, function (o, n)
  for _ = 1, n do o.b = o.i end
end, function (s) return s.b end, true)

run("int32->bool false", function (s) s.i = 0 end, function (o, n)
  for _ = 1, n do o.b = o.i end
end, function (s) return s.b end, false)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/235_j2_bool_int_cross_copy ok")
