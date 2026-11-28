-- ffi/276_j2_int_field_float_fill.lua -- J2 integer field fills from cdata
-- floating-point scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/276_j2_int_field_float_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t j; int64_t z; } NF276;
]]

local function run (name, scalar, f, get, want)
  local s = ffi.new("NF276"); s.i, s.j, s.z = 0, 0, 4
  local v = ffi.new(scalar)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, v, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local V = ffi.new("double", -3.9)
  local s = ffi.new("NF276"); s.i, s.j, s.z = 0, 0, 4
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int64<-double did not run")
  H.eq(s.i, -3, "int64<-double")
  H.eq(s.z, 4, "int64 sibling")
end

do
  local V = ffi.new("double", -3.9)
  local s = ffi.new("NF276"); s.i, s.j, s.z = 0, 0, 4
  local function fill (o, v, n) for _ = 1, n do o.j = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int32<-double did not run")
  H.eq(s.j, -3, "int32<-double")
  H.eq(s.z, 4, "int32 sibling")
end

do
  local U = ffi.new("double", 7.9)
  local s = ffi.new("NF276"); s.i, s.j, s.z = 0, 0, 4
  local function fill (n) for _ = 1, n do s.i = U end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue int64 did not run")
  H.eq(s.i, 7, "upvalue int64")
  H.eq(s.z, 4, "upvalue sibling")
end

do
  local V = ffi.new("float", 2.5)
  local s = ffi.new("NF276"); s.i, s.j, s.z = 0, 0, 4
  local function fill (o, v, n) for _ = 1, n do o.j = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int32<-float did not run")
  H.eq(s.j, 2, "int32<-float")
  H.eq(s.z, 4, "float sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/276_j2_int_field_float_fill ok")
