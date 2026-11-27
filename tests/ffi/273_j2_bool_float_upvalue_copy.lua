-- ffi/273_j2_bool_float_upvalue_copy.lua -- J2 bool/float mixed copies with
-- an upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/273_j2_bool_float_upvalue_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; float f; int64_t z; } NF273;
]]

local function check (name, s, b, f)
  H.eq(s.b, b, name .. ": b")
  H.eq(s.f, f, name .. ": f")
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local U = ffi.new("NF273"); U.b, U.f, U.z = true, 0, 4
  local D = ffi.new("NF273"); D.b, D.f, D.z = false, 0, 4
  local function copy (o, n) for _ = 1, n do o.f = U.b end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "bool->float source upvalue did not run")
  check("bool->float source upvalue", D, false, 1.0)
end

do
  local U = ffi.new("NF273"); U.b, U.f, U.z = false, 0, 4
  local D = ffi.new("NF273"); D.b, D.f, D.z = false, -3.9, 4
  local function copy (o, n) for _ = 1, n do U.b = o.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "float->bool destination upvalue did not run")
  check("float->bool destination upvalue", U, true, 0)
end

do
  local U = ffi.new("NF273"); U.b, U.f, U.z = true, 0, 4
  local function copy (n) for _ = 1, n do U.f = U.b end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "both upvalues did not run")
  check("both upvalues", U, true, 1.0)
end

do
  local U = ffi.new("NF273"); U.b, U.f, U.z = false, 0.0, 4
  local function copy (n) for _ = 1, n do U.b = U.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "zero->bool did not run")
  check("zero->bool", U, false, 0.0)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/273_j2_bool_float_upvalue_copy ok")
