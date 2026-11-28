-- ffi/306_j2_lua_number_uint64_upvalue.lua -- J2 uint64 fills from Lua
-- numbers held in upvalues.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/306_j2_lua_number_uint64_upvalue: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; int64_t i; int32_t z; } NF306;
]]

do
  local s = ffi.new("NF306"); s.u, s.i, s.z = 0, 0, 4
  local V = 9223372036854775808.0
  local function fill (n) for _ = 1, n do s.u = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "2^63 did not run")
  H.check(s.u == ffi.new("uint64_t", 0x8000000000000000), "2^63")
  H.eq(s.z, 4, "2^63 sibling")
end

do
  local s = ffi.new("NF306"); s.u, s.i, s.z = 0, 0, 4
  local V = -1.0
  local function fill (n) for _ = 1, n do s.u = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "-1 did not run")
  H.check(s.u == ffi.new("uint64_t", -1), "-1")
  H.eq(s.z, 4, "-1 sibling")
end

do
  local s = ffi.new("NF306"); s.u, s.i, s.z = 0, 0, 4
  local V = 1e18
  local function fill (o, n) for _ = 1, n do o.u = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "1e18 did not run")
  H.check(s.u == ffi.new("uint64_t", 1000000000000000000), "1e18")
  H.eq(s.z, 4, "1e18 sibling")
end

do
  local s = ffi.new("NF306"); s.u, s.i, s.z = 0, 0, 4
  local V = 9007199254740992.0
  local function fill (n) for _ = 1, n do s.i = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "2^53 did not run")
  H.check(s.i == ffi.new("int64_t", 9007199254740992), "2^53")
  H.eq(s.z, 4, "2^53 sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/306_j2_lua_number_uint64_upvalue ok")
