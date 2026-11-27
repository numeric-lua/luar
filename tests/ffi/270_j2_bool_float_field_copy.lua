-- ffi/270_j2_bool_float_field_copy.lua -- J2 bool/float mixed field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/270_j2_bool_float_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; float f; int64_t z; } NF270;
]]

local function run (name, setup, f, check)
  local s = ffi.new("NF270"); s.b, s.f, s.z = false, 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  check(s)
  H.eq(s.z, 4, name .. ": sibling")
end

run("true->float", function (s) s.b = true end, function (o, n)
  for _ = 1, n do o.f = o.b end
end, function (s) H.eq(s.f, 1.0, "true->float") end)

run("false->float", function (s) s.b = false end, function (o, n)
  for _ = 1, n do o.f = o.b end
end, function (s) H.eq(s.f, 0.0, "false->float") end)

run("nonzero->bool", function (s) s.f = -3.9 end, function (o, n)
  for _ = 1, n do o.b = o.f end
end, function (s) H.eq(s.b, true, "nonzero->bool") end)

run("zero->bool", function (s) s.f = 0.0 end, function (o, n)
  for _ = 1, n do o.b = o.f end
end, function (s) H.eq(s.b, false, "zero->bool") end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/270_j2_bool_float_field_copy ok")
