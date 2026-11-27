-- ffi/261_j2_f32_upvalue_field_scalar_update.lua -- J2 float field updates
-- on an upvalue struct from numeric scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/261_j2_f32_upvalue_field_scalar_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { float x; int64_t d; } NF261;
]]

local function run (name, value, f)
  local w = ffi.new("NF261"); w.x, w.d = 1, 4
  jit.off(); f(w, value, 200)
  local want = w.x

  local U = ffi.new("NF261"); U.x, U.d = 1, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(U, value, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(U.x, want, name .. ": value")
  H.eq(U.d, 4, name .. ": sibling")
end

run("add", 2.5, function (o, v, n)
  for _ = 1, n do o.x = o.x + v end
end)
run("subtract", 2.5, function (o, v, n)
  for _ = 1, n do o.x = o.x - v end
end)
run("multiply", 2.5, function (o, v, n)
  for _ = 1, n do o.x = o.x * v end
end)
run("divide", 2.0, function (o, v, n)
  for _ = 1, n do o.x = o.x / v end
end)
run("modulo", 2.0, function (o, v, n)
  for _ = 1, n do o.x = o.x % v end
end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/261_j2_f32_upvalue_field_scalar_update ok")
