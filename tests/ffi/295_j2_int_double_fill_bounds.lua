-- ffi/295_j2_int_double_fill_bounds.lua -- J2 integer field fills near the
-- signed/unsigned 64-bit boundaries.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/295_j2_int_double_fill_bounds: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; int64_t i; int32_t z; } NF295;
]]

local function run (name, f, check)
  local s = ffi.new("NF295"); s.u, s.i, s.z = 0, 0, 4
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  check(s)
  H.eq(s.z, 4, name .. ": sibling")
end

run("2^63 uint64", function (o, n)
  local V = ffi.new("double", 9223372036854775808.0)
  for _ = 1, n do o.u = V end
end, function (s)
  H.check(s.u == ffi.new("uint64_t", 0x8000000000000000), "2^63")
end)

run("1e18 uint64", function (o, n)
  local V = ffi.new("double", 1e18)
  for _ = 1, n do o.u = V end
end, function (s)
  H.check(s.u == ffi.new("uint64_t", 1000000000000000000), "1e18")
end)

run("-9e18 int64", function (o, n)
  local V = ffi.new("double", -9e18)
  for _ = 1, n do o.i = V end
end, function (s)
  H.check(s.i == ffi.new("int64_t", -9000000000000000000), "-9e18")
end)

do
  local s = ffi.new("NF295"); s.u, s.i, s.z = 0, 0, 4
  local V = ffi.new("double", 9223372036854775808.0)
  local function fill (n) for _ = 1, n do s.u = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.check(s.u == ffi.new("uint64_t", 0x8000000000000000), "upvalue")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/295_j2_int_double_fill_bounds ok")
