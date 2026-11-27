-- ffi/251_j2_f32_field_scalar_fill.lua -- J2 float field fills from numeric
-- scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/251_j2_f32_field_scalar_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { float x; int64_t d; } NF251;
]]

local function run (name, want, f, get)
  local s = ffi.new("NF251")
  s.x, s.d = 0, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": sibling")
end

run("number", 2.5, function (o, n)
  local v = 2.5
  for _ = 1, n do o.x = v end
end, function (s) return s.x end)

run("negative", -3.25, function (o, n)
  local v = -3.25
  for _ = 1, n do o.x = v end
end, function (s) return s.x end)

run("integer", 7.0, function (o, n)
  local v = 7
  for _ = 1, n do o.x = v end
end, function (s) return s.x end)

do
  local s = ffi.new("NF251"); s.x, s.d = 0, 4
  local V = ffi.new("float", 9.5)
  local function fill (o, v, n) for _ = 1, n do o.x = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "cdata did not run")
  H.eq(s.x, 9.5, "cdata value")
  H.eq(s.d, 4, "cdata sibling")
end

do
  local U = ffi.new("NF251"); U.x, U.d = 0, 4
  local V = 4.5
  local function fill (n) for _ = 1, n do U.x = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(U.x, 4.5, "upvalue value")
  H.eq(U.d, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/251_j2_f32_field_scalar_fill ok")
