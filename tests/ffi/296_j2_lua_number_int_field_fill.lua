-- ffi/296_j2_lua_number_int_field_fill.lua -- J2 integer field fills from Lua
-- numbers across the full uint64 range.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/296_j2_lua_number_int_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; int64_t i; int32_t j; int32_t z; } NF296;
]]

local function run (name, f, check)
  local s = ffi.new("NF296"); s.u, s.i, s.j, s.z = 0, 0, 0, 4
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  check(s)
  H.eq(s.z, 4, name .. ": sibling")
end

run("wide uint64", function (o, n)
  local V = 1e19
  for _ = 1, n do o.u = V end
end, function (s)
  H.check(s.u == ffi.new("uint64_t", 10000000000000000000), "wide")
end)

run("fraction uint64", function (o, n)
  local V = 42.9
  for _ = 1, n do o.u = V end
end, function (s)
  H.check(s.u == ffi.new("uint64_t", 42), "fraction")
end)

run("negative int64", function (o, n)
  local V = -3.9
  for _ = 1, n do o.i = V end
end, function (s)
  H.check(s.i == ffi.new("int64_t", -3), "negative")
end)

do
  local s = ffi.new("NF296"); s.u, s.i, s.j, s.z = 0, 0, 0, 4
  local V = 7.9
  local function fill (n) for _ = 1, n do s.j = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(s.j, 7, "upvalue")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/296_j2_lua_number_int_field_fill ok")
