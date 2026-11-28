-- ffi/288_j2_bool_enum_float_update.lua -- J2 float/double field updates from
-- cdata bool/enum scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/288_j2_bool_enum_float_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EEU288 = 1 } EE288;
typedef struct { double d; float f; int32_t z; } NF288;
]]

do
  local s = ffi.new("NF288"); s.d, s.f, s.z = 1, 1, 4
  local V = ffi.new("EE288", 7)
  local function fill (o, v, n) for _ = 1, n do o.d = o.d + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "enum add double did not run")
  H.eq(s.d, 1401.0, "enum add double")
  H.eq(s.z, 4, "enum sibling")
end

do
  local s = ffi.new("NF288"); s.d, s.f, s.z = 1, 1, 4
  local V = ffi.new("EE288", 3)
  local function fill (o, v, n) for _ = 1, n do o.f = o.f + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "enum add float did not run")
  H.eq(s.f, 601.0, "enum add float")
  H.eq(s.z, 4, "enum float sibling")
end

do
  local U = ffi.new("NF288"); U.d, U.f, U.z = 1, 1, 4
  local V = ffi.new("_Bool", true)
  local function fill (n) for _ = 1, n do U.d = U.d + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue bool did not run")
  H.eq(U.d, 201.0, "upvalue bool")
  H.eq(U.z, 4, "upvalue sibling")
end

do
  local s = ffi.new("NF288"); s.d, s.f, s.z = 1, 1, 4
  local V = ffi.new("_Bool", false)
  local function fill (o, v, n) for _ = 1, n do o.f = o.f - v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "bool sub did not run")
  H.eq(s.f, 1.0, "bool sub")
  H.eq(s.z, 4, "bool sub sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/288_j2_bool_enum_float_update ok")
