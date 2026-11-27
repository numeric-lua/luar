-- ffi/269_j2_mixed_int64_upvalue_copy.lua -- J2 mixed int64/float field
-- copies with an upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/269_j2_mixed_int64_upvalue_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; double d; float f; int32_t z; } NF269;
]]

local function check (name, s, field, want)
  H.eq(s[field], want, name .. ": " .. field)
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local U = ffi.new("NF269"); U.i, U.d, U.f, U.z = -9000000000, 0, 0, 4
  local D = ffi.new("NF269"); D.i, D.d, D.f, D.z = 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.d = U.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "source upvalue did not run")
  check("source upvalue", D, "d", -9000000000.0)
end

do
  local U = ffi.new("NF269"); U.i, U.d, U.f, U.z = 0, 0, 0, 4
  local D = ffi.new("NF269"); D.i, D.d, D.f, D.z = 3, 0, 0, 4
  local function copy (o, n) for _ = 1, n do U.d = o.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "destination upvalue did not run")
  check("destination upvalue", U, "d", 3.0)
end

do
  local U = ffi.new("NF269"); U.i, U.d, U.f, U.z = -9000000000, 0, 0, 4
  local function copy (n) for _ = 1, n do U.d = U.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "both upvalues did not run")
  check("both upvalues", U, "d", -9000000000.0)
end

do
  local U = ffi.new("NF269"); U.i, U.d, U.f, U.z = 0, 0, -3.9, 4
  local D = ffi.new("NF269"); D.i, D.d, D.f, D.z = 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.i = U.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "float->int64 upvalue did not run")
  check("float->int64 upvalue", D, "i", -3)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/269_j2_mixed_int64_upvalue_copy ok")
