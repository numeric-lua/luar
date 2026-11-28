-- ffi/286_j2_bool_enum_int_field_update.lua -- J2 integer field updates from
-- cdata bool/enum scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/286_j2_bool_enum_int_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EUI286 = 1 } EU286;
typedef struct { int64_t i; int32_t j; int32_t z; } NF286;
]]

do
  local s = ffi.new("NF286"); s.i, s.j, s.z = 1, 1, 4
  local V = ffi.new("_Bool", true)
  local function fill (o, v, n) for _ = 1, n do o.i = o.i + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "bool add did not run")
  H.eq(s.i, 201, "bool add")
  H.eq(s.z, 4, "bool add sibling")
end

do
  local s = ffi.new("NF286"); s.i, s.j, s.z = 1, 1, 4
  local V = ffi.new("_Bool", false)
  local function fill (o, v, n) for _ = 1, n do o.i = o.i - v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "bool sub did not run")
  H.eq(s.i, 1, "bool sub")
  H.eq(s.z, 4, "bool sub sibling")
end

do
  local s = ffi.new("NF286"); s.i, s.j, s.z = 1, 1, 4
  local V = ffi.new("EU286", 7)
  local function fill (o, v, n) for _ = 1, n do o.j = o.j + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "enum add did not run")
  H.eq(s.j, 1401, "enum add")
  H.eq(s.z, 4, "enum add sibling")
end

do
  local U = ffi.new("NF286"); U.i, U.j, U.z = 1, 1, 4
  local V = ffi.new("_Bool", true)
  local function fill (n) for _ = 1, n do U.i = U.i + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(U.i, 201, "upvalue")
  H.eq(U.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/286_j2_bool_enum_int_field_update ok")
