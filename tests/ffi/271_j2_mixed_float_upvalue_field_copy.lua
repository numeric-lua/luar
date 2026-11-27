-- ffi/271_j2_mixed_float_upvalue_field_copy.lua -- J2 mixed float/double
-- copies with an upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/271_j2_mixed_float_upvalue_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { float f; double d; int64_t z; } NF271;
]]

local function check (name, s, f, d)
  H.eq(s.f, f, name .. ": f")
  H.eq(s.d, d, name .. ": d")
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local U = ffi.new("NF271"); U.f, U.d, U.z = 2.5, 0, 4
  local D = ffi.new("NF271"); D.f, D.d, D.z = 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.d = U.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "source upvalue did not run")
  check("source upvalue", D, 0, 2.5)
end

do
  local U = ffi.new("NF271"); U.f, U.d, U.z = 0, 0, 4
  local D = ffi.new("NF271"); D.f, D.d, D.z = 0, -3.25, 4
  local function copy (o, n) for _ = 1, n do U.f = o.d end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "destination upvalue did not run")
  check("destination upvalue", U, -3.25, 0)
end

do
  local U = ffi.new("NF271"); U.f, U.d, U.z = 2.5, 0, 4
  local function copy (n) for _ = 1, n do U.d = U.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "both upvalues did not run")
  check("both upvalues", U, 2.5, 2.5)
end

do
  local U = ffi.new("NF271"); U.f, U.d, U.z = 0, -3.25, 4
  local function copy (n) for _ = 1, n do U.f = U.d end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "negative f64->f32 did not run")
  check("negative f64->f32", U, -3.25, -3.25)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/271_j2_mixed_float_upvalue_field_copy ok")
