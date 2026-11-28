-- ffi/292_j2_int64_bounds.lua -- J2 int64 field bounds and wrap.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/292_j2_int64_bounds: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t z; } NF292;
]]

local IMIN = -0x7fffffffffffffff - 1
local IMAX = 0x7fffffffffffffff

local function run (name, setup, f, want)
  local s = ffi.new("NF292"); s.i, s.z = 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.check(s.i == ffi.new("int64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("min fill", function (s) end, function (o, n)
  local V = ffi.new("int64_t", IMIN)
  for _ = 1, n do o.i = V end
end, IMIN)

run("max fill", function (s) end, function (o, n)
  local V = ffi.new("int64_t", IMAX)
  for _ = 1, n do o.i = V end
end, IMAX)

do
  local s = ffi.new("NF292"); s.i, s.z = IMAX, 4
  local V = ffi.new("int64_t", 1)
  local function fill (o, v, n) for _ = 1, n do o.i = o.i + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "wrap did not run")
  H.check(s.i == IMIN + 199,
          "wrap value")
  H.eq(s.z, 4, "wrap sibling")
end

do
  local s = ffi.new("NF292"); s.i, s.z = 0, 4
  local V = ffi.new("int64_t", IMIN)
  local function fill (n) for _ = 1, n do s.i = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.check(s.i == ffi.new("int64_t", IMIN),
          "upvalue value")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/292_j2_int64_bounds ok")
