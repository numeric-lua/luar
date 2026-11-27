-- ffi/268_j2_mixed_int64_float_field_copy.lua -- J2 mixed int64/float field
-- copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/268_j2_mixed_int64_float_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; double d; float f; int32_t z; } NF268;
]]

local function run (name, setup, f, get, want)
  local s = ffi.new("NF268"); s.i, s.d, s.f, s.z = 1, 0, 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("int64->double", function (s) s.i = -9000000000 end, function (o, n)
  for _ = 1, n do o.d = o.i end
end, function (s) return s.d end, -9000000000.0)

run("double->int64", function (s) s.d = 3.9 end, function (o, n)
  for _ = 1, n do o.i = o.d end
end, function (s) return s.i end, 3)

run("int64->float", function (s) s.i = -7000000000 end, function (o, n)
  for _ = 1, n do o.f = o.i end
end, function (s) return s.f end, -7000000000.0)

run("float->int64", function (s) s.f = -3.9 end, function (o, n)
  for _ = 1, n do o.i = o.f end
end, function (s) return s.i end, -3)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/268_j2_mixed_int64_float_field_copy ok")
