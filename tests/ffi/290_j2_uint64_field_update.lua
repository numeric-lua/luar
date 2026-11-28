-- ffi/290_j2_uint64_field_update.lua -- J2 unsigned 64-bit field updates.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/290_j2_uint64_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; int32_t z; } NF290;
]]

local function run (name, f, get, want)
  local s = ffi.new("NF290"); s.u, s.z = 1, 4
  local V = ffi.new("uint64_t", 2)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, V, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.check(s.u == ffi.new("uint64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("add", function (o, v, n)
  for _ = 1, n do o.u = o.u + v end
end, function (s) return tonumber(s.u) end, 401)

run("sub", function (o, v, n)
  for _ = 1, n do o.u = o.u - v end
end, function (s) return tonumber(s.u) end, -399)

do
  local U = ffi.new("NF290"); U.u, U.z = 1, 4
  local V = ffi.new("uint64_t", 3)
  local function fill (n) for _ = 1, n do U.u = U.u + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.check(U.u == ffi.new("uint64_t", 601), "upvalue value")
  H.eq(U.z, 4, "upvalue sibling")
end

do
  local s = ffi.new("NF290"); s.u, s.z = -1, 4
  local V = ffi.new("uint64_t", 1)
  local function fill (o, v, n) for _ = 1, n do o.u = o.u + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "wrap did not run")
  H.check(s.u == ffi.new("uint64_t", 199), "wrap")
  H.eq(s.z, 4, "wrap sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/290_j2_uint64_field_update ok")
