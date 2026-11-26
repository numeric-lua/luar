-- ffi/242_j2_f64_field_fill.lua -- J2 double field fills from numeric
-- constants.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/242_j2_f64_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { double x; float y; int64_t d; } NDF;
]]

local function run (name, value, want, f, get)
  local s = ffi.new("NDF")
  s.x, s.y, s.d = 0, 2, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": sibling")
end

run("float", 2.5, 2.5, function (o, n)
  for _ = 1, n do o.x = 2.5 end
end, function (s) return s.x end)

run("negative", -3.25, -3.25, function (o, n)
  for _ = 1, n do o.x = -3.25 end
end, function (s) return s.x end)

run("integer", 7, 7.0, function (o, n)
  for _ = 1, n do o.x = 7 end
end, function (s) return s.x end)

run("zero", 0, 0.0, function (o, n)
  for _ = 1, n do o.x = 0 end
end, function (s) return s.x end)

do
  local U = ffi.new("NDF"); U.x, U.y, U.d = 0, 2, 4
  local function fill (n) for _ = 1, n do U.x = 9.5 end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue fill did not run")
  H.eq(U.x, 9.5, "upvalue value")
  H.eq(U.d, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/242_j2_f64_field_fill ok")
