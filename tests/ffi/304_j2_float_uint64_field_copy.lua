-- ffi/304_j2_float_uint64_field_copy.lua -- J2 uint64 field copies from
-- floating cdata.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/304_j2_float_uint64_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; float f; double d; int32_t z; } NF304;
]]

local function run (name, setup, f, want)
  local s = ffi.new("NF304"); s.u, s.f, s.d, s.z = 0, 0, 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.check(s.u == ffi.new("uint64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("float 7", function (s) s.f = 7.0 end, function (o, n)
  for _ = 1, n do o.u = o.f end
end, 7)

run("float 2^63", function (s) s.f = 9223372036854775808.0 end, function (o, n)
  for _ = 1, n do o.u = o.f end
end, 0x8000000000000000)

run("double 42.9", function (s) s.d = 42.9 end, function (o, n)
  for _ = 1, n do o.u = o.d end
end, 42)

do
  local U = ffi.new("NF304"); U.u, U.f, U.d, U.z = 0, 9.5, 0, 4
  local D = ffi.new("NF304"); D.u, D.f, D.d, D.z = 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.u = U.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.check(D.u == ffi.new("uint64_t", 9), "upvalue value")
  H.eq(D.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/304_j2_float_uint64_field_copy ok")
