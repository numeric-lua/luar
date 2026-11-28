-- ffi/297_j2_lua_number_uint64_fill.lua -- J2 uint64 field fills from Lua
-- numbers at boundary values.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/297_j2_lua_number_uint64_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; int32_t z; } NF297;
]]

local function check (name, s, want)
  H.check(s.u == ffi.new("uint64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local s = ffi.new("NF297"); s.u, s.z = 0, 4
  local V = 9223372036854775808.0
  local function fill (o, v, n) for _ = 1, n do o.u = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "2^63 did not run")
  check("2^63", s, 0x8000000000000000)
end

do
  local s = ffi.new("NF297"); s.u, s.z = 0, 4
  local V = -1.0
  local function fill (o, v, n) for _ = 1, n do o.u = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "-1 did not run")
  check("-1", s, -1)
end

do
  local s = ffi.new("NF297"); s.u, s.z = 0, 4
  local V = 1e18
  local function fill (o, v, n) for _ = 1, n do o.u = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "1e18 did not run")
  check("1e18", s, 1000000000000000000)
end

do
  local s = ffi.new("NF297"); s.u, s.z = 0, 4
  local V = 42.9
  local function fill (n) for _ = 1, n do s.u = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  check("upvalue", s, 42)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/297_j2_lua_number_uint64_fill ok")
