-- ffi/305_j2_lua_number_int_fill_fractions.lua -- J2 integer fills from Lua
-- numbers truncate fractions toward zero.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/305_j2_lua_number_int_fill_fractions: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t z; } NF305;
]]

local function run (name, value, want)
  local s = ffi.new("NF305"); s.i, s.z = 1, 4
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, value, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.check(s.i == ffi.new("int64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("0.9", 0.9, 0)
run("-0.9", -0.9, 0)
run("-0.0", -0.0, 0)
run("2^53", 9007199254740992.0, 9007199254740992)

do
  local s = ffi.new("NF305"); s.i, s.z = 1, 4
  local V = -0.5
  local function fill (n) for _ = 1, n do s.i = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.check(s.i == ffi.new("int64_t", 0), "upvalue value")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/305_j2_lua_number_int_fill_fractions ok")
