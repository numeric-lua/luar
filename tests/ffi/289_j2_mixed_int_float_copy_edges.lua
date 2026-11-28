-- ffi/289_j2_mixed_int_float_copy_edges.lua -- J2 mixed integer/float copy
-- edge cases.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status ~= nil and jit.status() ~= true then
  print("ffi/289_j2_mixed_int_float_copy_edges: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint32_t u; int16_t s; double d; float f; int32_t z; } NF289;
]]

local function run (name, setup, f, get, want)
  local s = ffi.new("NF289"); s.u, s.s, s.d, s.f, s.z = 0, 0, 0, 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("uint32->double", function (s) s.u = 0xffffffff end, function (o, n)
  for _ = 1, n do o.d = o.u end
end, function (s) return s.d end, 4294967295.0)

run("int16->double", function (s) s.s = -3 end, function (o, n)
  for _ = 1, n do o.d = o.s end
end, function (s) return s.d end, -3.0)

run("double->uint32", function (s) s.d = -3.9 end, function (o, n)
  for _ = 1, n do o.u = o.d end
end, function (s) return s.u end, 4294967293)

do
  local U = ffi.new("NF289"); U.u, U.s, U.d, U.f, U.z = 0, -3, 0, 0, 4
  local D = ffi.new("NF289"); D.u, D.s, D.d, D.f, D.z = 0, 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.f = U.s end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(D.f, -3.0, "upvalue value")
  H.eq(D.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/289_j2_mixed_int_float_copy_edges ok")
