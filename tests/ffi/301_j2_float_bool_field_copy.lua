-- ffi/301_j2_float_bool_field_copy.lua -- J2 bool field copies from floating
-- cdata normalize truth values.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/301_j2_float_bool_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; double d; float f; int32_t z; } NF301;
]]

local function run (name, setup, f, want)
  local s = ffi.new("NF301"); s.b, s.d, s.f, s.z = false, 0, 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(s.b, want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("double 2.5", function (s) s.d = 2.5 end, function (o, n)
  for _ = 1, n do o.b = o.d end
end, true)

run("double 0", function (s) s.d = 0.0 end, function (o, n)
  for _ = 1, n do o.b = o.d end
end, false)

run("float -1", function (s) s.f = -1.0 end, function (o, n)
  for _ = 1, n do o.b = o.f end
end, true)

do
  local U = ffi.new("NF301"); U.b, U.d, U.f, U.z = false, 0, -3.5, 4
  local D = ffi.new("NF301"); D.b, D.d, D.f, D.z = false, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.b = U.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(D.b, true, "upvalue value")
  H.eq(D.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/301_j2_float_bool_field_copy ok")
