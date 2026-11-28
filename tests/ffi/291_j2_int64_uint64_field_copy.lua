-- ffi/291_j2_int64_uint64_field_copy.lua -- J2 int64/uint64 field copies
-- preserve raw bits.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/291_j2_int64_uint64_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; uint64_t u; int32_t z; } NF291;
]]

local function run (name, setup, f, want)
  local s = ffi.new("NF291"); s.i, s.u, s.z = 0, 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.check(s.u == ffi.new("uint64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("int64 -1 -> uint64", function (s) s.i = -1 end, function (o, n)
  for _ = 1, n do o.u = o.i end
end, -1)

run("int64 -3 -> uint64", function (s) s.i = -3 end, function (o, n)
  for _ = 1, n do o.u = o.i end
end, -3)

do
  local U = ffi.new("NF291"); U.i, U.u, U.z = -7, 0, 4
  local D = ffi.new("NF291"); D.i, D.u, D.z = 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.u = U.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.check(D.u == ffi.new("uint64_t", -7), "upvalue value")
  H.eq(D.z, 4, "upvalue sibling")
end

do
  local s = ffi.new("NF291"); s.i, s.u, s.z = 0, 0, 4
  local V = ffi.new("int64_t", -9)
  local function fill (o, v, n) for _ = 1, n do o.u = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "scalar did not run")
  H.check(s.u == ffi.new("uint64_t", -9), "scalar value")
  H.eq(s.z, 4, "scalar sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/291_j2_int64_uint64_field_copy ok")
