-- ffi/277_j2_int_field_float_update.lua -- J2 integer field updates from
-- cdata floating-point scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/277_j2_int_field_float_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t j; int64_t z; } NF277;
]]

local function run (name, f, get, want)
  local s = ffi.new("NF277"); s.i, s.j, s.z = 1, 1, 4
  local V = ffi.new("double", -3.9)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, V, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("int64 add", function (o, v, n)
  for _ = 1, n do o.i = o.i + v end
end, function (s) return s.i end, -599)

run("int32 sub", function (o, v, n)
  for _ = 1, n do o.j = o.j - v end
end, function (s) return s.j end, 601)

do
  local U = ffi.new("NF277"); U.i, U.j, U.z = 1, 1, 4
  local V = ffi.new("float", -3.9)
  local function fill (n) for _ = 1, n do U.i = U.i + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(U.i, -599, "upvalue value")
  H.eq(U.z, 4, "upvalue sibling")
end

do
  local s = ffi.new("NF277"); s.i, s.j, s.z = 1, 1, 4
  local V = ffi.new("float", 2.9)
  local function fill (o, v, n) for _ = 1, n do o.j = o.j + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "float scalar did not run")
  H.eq(s.j, 401, "float scalar")
  H.eq(s.z, 4, "float scalar sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/277_j2_int_field_float_update ok")
