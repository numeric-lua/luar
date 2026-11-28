-- ffi/285_j2_bool_enum_int_field_fill.lua -- J2 integer field fills from
-- cdata bool/enum scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/285_j2_bool_enum_int_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EIF285 = 1 } EI285;
typedef struct { int64_t i; int32_t j; int32_t z; } NF285;
]]

do
  local s = ffi.new("NF285"); s.i, s.j, s.z = 0, 0, 4
  local V = ffi.new("_Bool", true)
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "bool true did not run")
  H.eq(s.i, 1, "bool true")
  H.eq(s.z, 4, "bool true sibling")
end

do
  local s = ffi.new("NF285"); s.i, s.j, s.z = 0, 0, 4
  local V = ffi.new("_Bool", false)
  local function fill (o, v, n) for _ = 1, n do o.j = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "bool false did not run")
  H.eq(s.j, 0, "bool false")
  H.eq(s.z, 4, "bool false sibling")
end

do
  local s = ffi.new("NF285"); s.i, s.j, s.z = 0, 0, 4
  local V = ffi.new("EI285", 7)
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "enum did not run")
  H.eq(s.i, 7, "enum")
  H.eq(s.z, 4, "enum sibling")
end

do
  local s = ffi.new("NF285"); s.i, s.j, s.z = 0, 0, 4
  local V = ffi.new("_Bool", true)
  local function fill (n) for _ = 1, n do s.i = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(s.i, 1, "upvalue")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/285_j2_bool_enum_int_field_fill ok")
