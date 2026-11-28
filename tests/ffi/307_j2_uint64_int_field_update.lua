-- ffi/307_j2_uint64_int_field_update.lua -- J2 int64 field updates from
-- uint64 cdata.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/307_j2_uint64_int_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t z; } NF307;
]]

local function run (name, f, want)
  local s = ffi.new("NF307"); s.i, s.z = 1, 4
  local V = ffi.new("uint64_t", 2)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, V, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.check(s.i == ffi.new("int64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("add", function (o, v, n)
  for _ = 1, n do o.i = o.i + v end
end, 401)

run("sub", function (o, v, n)
  for _ = 1, n do o.i = o.i - v end
end, -399)

do
  local U = ffi.new("NF307"); U.i, U.z = 1, 4
  local V = ffi.new("uint64_t", 3)
  local function fill (n) for _ = 1, n do U.i = U.i + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.check(U.i == ffi.new("int64_t", 601), "upvalue value")
  H.eq(U.z, 4, "upvalue sibling")
end

do
  local s = ffi.new("NF307"); s.i, s.z = -1, 4
  local V = ffi.new("uint64_t", 1)
  local function fill (o, v, n) for _ = 1, n do o.i = o.i + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "wrap did not run")
  H.check(s.i == ffi.new("int64_t", 199), "wrap value")
  H.eq(s.z, 4, "wrap sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/307_j2_uint64_int_field_update ok")
