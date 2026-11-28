-- ffi/287_j2_bool_enum_float_fill.lua -- J2 float/double field fills from
-- cdata bool/enum scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/287_j2_bool_enum_float_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EEF287 = 1 } EE287;
typedef struct { double d; float f; int32_t z; } NF287;
]]

do
  local s = ffi.new("NF287"); s.d, s.f, s.z = 0, 0, 4
  local V = ffi.new("EE287", 7)
  local function fill (o, v, n) for _ = 1, n do o.d = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "enum->double did not run")
  H.eq(s.d, 7.0, "enum->double")
  H.eq(s.z, 4, "enum sibling")
end

do
  local s = ffi.new("NF287"); s.d, s.f, s.z = 0, 0, 4
  local V = ffi.new("EE287", 9)
  local function fill (o, v, n) for _ = 1, n do o.f = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "enum->float did not run")
  H.eq(s.f, 9.0, "enum->float")
  H.eq(s.z, 4, "enum float sibling")
end

do
  local s = ffi.new("NF287"); s.d, s.f, s.z = 0, 0, 4
  local V = ffi.new("_Bool", true)
  local function fill (o, v, n) for _ = 1, n do o.d = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "bool->double did not run")
  H.eq(s.d, 1.0, "bool->double")
  H.eq(s.z, 4, "bool sibling")
end

do
  local s = ffi.new("NF287"); s.d, s.f, s.z = 0, 0, 4
  local V = ffi.new("_Bool", false)
  local function fill (n) for _ = 1, n do s.f = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue bool did not run")
  H.eq(s.f, 0.0, "upvalue bool")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/287_j2_bool_enum_float_fill ok")
