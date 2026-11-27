-- ffi/267_j2_mixed_num_upvalue_field_copy.lua -- J2 mixed integer/float field
-- copies with an upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/267_j2_mixed_num_upvalue_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t i; double d; float f; int8_t c; int64_t z; } NF267;
]]

local function check (name, s, field, want)
  H.eq(s[field], want, name .. ": " .. field)
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local U = ffi.new("NF267"); U.i, U.d, U.f, U.c, U.z = -7, 0, 0, 0, 4
  local D = ffi.new("NF267"); D.i, D.d, D.f, D.c, D.z = 0, 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.d = U.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "source upvalue did not run")
  check("source upvalue", D, "d", -7.0)
end

do
  local U = ffi.new("NF267"); U.i, U.d, U.f, U.c, U.z = 0, 0, 0, 0, 4
  local D = ffi.new("NF267"); D.i, D.d, D.f, D.c, D.z = 3, 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do U.d = o.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "destination upvalue did not run")
  check("destination upvalue", U, "d", 3.0)
end

do
  local U = ffi.new("NF267"); U.i, U.d, U.f, U.c, U.z = -7, 0, 0, 0, 4
  local function copy (n) for _ = 1, n do U.d = U.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "both upvalues did not run")
  check("both upvalues", U, "d", -7.0)
end

do
  local U = ffi.new("NF267"); U.i, U.d, U.f, U.c, U.z = 0, 0, -3.9, 0, 4
  local D = ffi.new("NF267"); D.i, D.d, D.f, D.c, D.z = 0, 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.c = U.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "float->int upvalue did not run")
  check("float->int upvalue", D, "c", -3)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/267_j2_mixed_num_upvalue_field_copy ok")
