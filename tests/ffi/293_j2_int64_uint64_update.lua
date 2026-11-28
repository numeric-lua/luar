-- ffi/293_j2_int64_uint64_update.lua -- J2 uint64 field updates from int64
-- cdata use two's-complement bits.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/293_j2_int64_uint64_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; int32_t z; } NF293;
]]

local function check (name, s, want)
  H.check(s.u == ffi.new("uint64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local s = ffi.new("NF293"); s.u, s.z = 1, 4
  local V = ffi.new("int64_t", -1)
  local function fill (o, v, n) for _ = 1, n do o.u = o.u + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "add did not run")
  check("add", s, -199)
end

do
  local s = ffi.new("NF293"); s.u, s.z = 1, 4
  local V = ffi.new("int64_t", -1)
  local function fill (o, v, n) for _ = 1, n do o.u = o.u - v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "sub did not run")
  check("sub", s, 201)
end

do
  local U = ffi.new("NF293"); U.u, U.z = 1, 4
  local V = ffi.new("int64_t", -3)
  local function fill (n) for _ = 1, n do U.u = U.u + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  check("upvalue", U, -599)
end

do
  local s = ffi.new("NF293"); s.u, s.z = 0, 4
  local V = ffi.new("int64_t", -7)
  local function fill (o, v, n) for _ = 1, n do o.u = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "fill did not run")
  check("fill", s, -7)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/293_j2_int64_uint64_update ok")
