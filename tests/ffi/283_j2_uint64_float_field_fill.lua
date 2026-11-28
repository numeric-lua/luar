-- ffi/283_j2_uint64_float_field_fill.lua -- J2 float/double field fills from
-- uint64 cdata.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/283_j2_uint64_float_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { double d; float f; int32_t z; } NF283;
]]

do
  local s = ffi.new("NF283"); s.d, s.f, s.z = 0, 0, 4
  local V = ffi.new("uint64_t", -1)
  local function fill (o, v, n) for _ = 1, n do o.d = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "double did not run")
  H.eq(s.d, 18446744073709551616.0, "double")
  H.eq(s.z, 4, "double sibling")
end

do
  local s = ffi.new("NF283"); s.d, s.f, s.z = 0, 0, 4
  local V = ffi.new("uint64_t", 0x8000000000000000)
  local function fill (o, v, n) for _ = 1, n do o.f = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "float did not run")
  H.eq(s.f, 9223372036854775808.0, "float")
  H.eq(s.z, 4, "float sibling")
end

do
  local s = ffi.new("NF283"); s.d, s.f, s.z = 0, 0, 4
  local V = ffi.new("uint64_t", 42)
  local function fill (n) for _ = 1, n do s.d = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(s.d, 42.0, "upvalue")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/283_j2_uint64_float_field_fill ok")
