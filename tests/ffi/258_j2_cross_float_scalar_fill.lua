-- ffi/258_j2_cross_float_scalar_fill.lua -- J2 float/double field fills from
-- the other cdata float width.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/258_j2_cross_float_scalar_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { float f; double d; int64_t z; } NF258;
]]

local function run (name, f, get, want)
  local s = ffi.new("NF258"); s.f, s.d, s.z = 0, 0, 4
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local V = ffi.new("float", 2.5)
  run("double<-float", function (o, n)
    for _ = 1, n do o.d = V end
  end, function (s) return s.d end, 2.5)
end

do
  local V = ffi.new("double", -3.25)
  run("float<-double", function (o, n)
    for _ = 1, n do o.f = V end
  end, function (s) return s.f end, -3.25)
end

do
  local U = ffi.new("NF258"); U.f, U.d, U.z = 0, 0, 4
  local V = ffi.new("float", 4.5)
  local function fill (n) for _ = 1, n do U.d = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue double<-float did not run")
  H.eq(U.d, 4.5, "upvalue double")
  H.eq(U.z, 4, "upvalue sibling")
end

do
  local U = ffi.new("NF258"); U.f, U.d, U.z = 0, 0, 4
  local V = ffi.new("double", 9.5)
  local function fill (n) for _ = 1, n do U.f = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue float<-double did not run")
  H.eq(U.f, 9.5, "upvalue float")
  H.eq(U.z, 4, "upvalue sibling 2")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/258_j2_cross_float_scalar_fill ok")
