-- ffi/249_j2_f64_upvalue_field_copy.lua -- J2 double field copies with an
-- upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/249_j2_f64_upvalue_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { double x; double y; int64_t d; } NF249;
]]

local function check (name, s, x, y)
  H.eq(s.x, x, name .. ": x")
  H.eq(s.y, y, name .. ": y")
  H.eq(s.d, 4, name .. ": sibling")
end

do
  local U = ffi.new("NF249"); U.x, U.y, U.d = 1, 2.5, 4
  local D = ffi.new("NF249"); D.x, D.y, D.d = 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.x = U.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "source upvalue did not run")
  check("source upvalue", D, 2.5, 0)
end

do
  local U = ffi.new("NF249"); U.x, U.y, U.d = 0, 0, 4
  local D = ffi.new("NF249"); D.x, D.y, D.d = 0, 9.5, 4
  local function copy (o, n) for _ = 1, n do U.x = o.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "destination upvalue did not run")
  check("destination upvalue", U, 9.5, 0)
end

do
  local U = ffi.new("NF249"); U.x, U.y, U.d = 1, 2.5, 4
  local function copy (n) for _ = 1, n do U.x = U.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "both upvalues did not run")
  check("both upvalues", U, 2.5, 2.5)
end

do
  local U = ffi.new("NF249"); U.x, U.y, U.d = 3.5, 2.5, 4
  local function copy (n) for _ = 1, n do U.x = U.x end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "self did not run")
  check("self", U, 3.5, 2.5)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/249_j2_f64_upvalue_field_copy ok")
