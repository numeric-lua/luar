-- ffi/259_j2_int_cdata_float_field_fill.lua -- J2 float/double field fills
-- from cdata integer scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/259_j2_int_cdata_float_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { double d; float f; int64_t z; } NF259;
]]

local function new ()
  local s = ffi.new("NF259"); s.d, s.f, s.z = 0, 0, 4
  return s
end

do
  local s = new()
  local V = ffi.new("int32_t", -7)
  local function fill (o, v, n) for _ = 1, n do o.d = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "double negative did not run")
  H.eq(s.d, -7, "double negative")
  H.eq(s.z, 4, "double sibling")
end

do
  local s = new()
  local V = ffi.new("int32_t", -7)
  local function fill (o, v, n) for _ = 1, n do o.f = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "float negative did not run")
  H.eq(s.f, -7, "float negative")
  H.eq(s.z, 4, "float sibling")
end

do
  local s = new()
  local V = ffi.new("uint32_t", 0xffffffff)
  local function fill (o, v, n) for _ = 1, n do o.d = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "uint32 did not run")
  H.eq(s.d, 4294967295.0, "uint32")
  H.eq(s.z, 4, "uint32 sibling")
end

do
  local s = new()
  local V = ffi.new("int32_t", 9)
  local function fill (n) for _ = 1, n do s.d = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(s.d, 9.0, "upvalue")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/259_j2_int_cdata_float_field_fill ok")
