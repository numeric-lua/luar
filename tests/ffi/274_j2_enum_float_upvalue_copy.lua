-- ffi/274_j2_enum_float_upvalue_copy.lua -- J2 enum/float mixed copies with
-- an upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/274_j2_enum_float_upvalue_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EFA274 = 1 } EF274;
typedef struct { EF274 e; float f; double d; int64_t z; } NF274;
]]

local function check (name, s, e, d)
  H.eq(s.e, e, name .. ": e")
  H.eq(s.d, d, name .. ": d")
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local U = ffi.new("NF274"); U.e, U.f, U.d, U.z = 7, 0, 0, 4
  local D = ffi.new("NF274"); D.e, D.f, D.d, D.z = 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.d = U.e end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "enum source upvalue did not run")
  check("enum source upvalue", D, 0, 7.0)
end

do
  local U = ffi.new("NF274"); U.e, U.f, U.d, U.z = 0, 0, 0, 4
  local D = ffi.new("NF274"); D.e, D.f, D.d, D.z = 0, 0, 9.9, 4
  local function copy (o, n) for _ = 1, n do U.e = o.d end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "enum destination upvalue did not run")
  check("enum destination upvalue", U, 9, 0)
end

do
  local U = ffi.new("NF274"); U.e, U.f, U.d, U.z = 5, 0, 0, 4
  local function copy (n) for _ = 1, n do U.d = U.e end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "both upvalues did not run")
  check("both upvalues", U, 5, 5.0)
end

do
  local U = ffi.new("NF274"); U.e, U.f, U.d, U.z = 0, 0, -3.9, 4
  local function copy (n) for _ = 1, n do U.e = U.d end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "negative double->enum did not run")
  check("negative double->enum", U, 4294967293, -3.9)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/274_j2_enum_float_upvalue_copy ok")
