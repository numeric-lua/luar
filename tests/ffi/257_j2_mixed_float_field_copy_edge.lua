-- ffi/257_j2_mixed_float_field_copy_edge.lua -- J2 mixed float/double edge
-- values.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/257_j2_mixed_float_field_copy_edge: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { float f; double d; int64_t z; } NF257;
]]

local function run (name, f, check)
  local s = ffi.new("NF257"); s.f, s.d, s.z = 0, 0, 4
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  check(s)
  H.eq(s.z, 4, name .. ": sibling")
end

run("negative f32->f64", function (o, n)
  o.f = -3.25
  for _ = 1, n do o.d = o.f end
end, function (s) H.eq(s.d, -3.25, "negative") end)

run("large f64->f32", function (o, n)
  o.d = 1e300
  for _ = 1, n do o.f = o.d end
end, function (s) H.eq(s.f, 1/0, "large") end)

run("nan f64->f32", function (o, n)
  o.d = 0/0
  for _ = 1, n do o.f = o.d end
end, function (s) H.check(s.f ~= s.f, "nan") end)

run("inf f32->f64", function (o, n)
  o.f = 1/0
  for _ = 1, n do o.d = o.f end
end, function (s) H.eq(s.d, 1/0, "inf") end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/257_j2_mixed_float_field_copy_edge ok")
