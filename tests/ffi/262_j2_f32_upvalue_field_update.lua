-- ffi/262_j2_f32_upvalue_field_update.lua -- J2 float field updates on an
-- upvalue struct with constants.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/262_j2_f32_upvalue_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { float x; int64_t d; } NF262;
]]

local function run (name, f)
  local w = ffi.new("NF262"); w.x, w.d = 1, 4
  jit.off(); f(w, 200)
  local want = w.x

  local U = ffi.new("NF262"); U.x, U.d = 1, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(U, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(U.x, want, name .. ": value")
  H.eq(U.d, 4, name .. ": sibling")
end

run("add", function (o, n)
  for _ = 1, n do o.x = o.x + 2.5 end
end)
run("subtract", function (o, n)
  for _ = 1, n do o.x = o.x - 2.5 end
end)
run("multiply", function (o, n)
  for _ = 1, n do o.x = o.x * 2.5 end
end)
run("divide", function (o, n)
  for _ = 1, n do o.x = o.x / 2.0 end
end)
run("modulo", function (o, n)
  for _ = 1, n do o.x = o.x % 2.0 end
end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/262_j2_f32_upvalue_field_update ok")
