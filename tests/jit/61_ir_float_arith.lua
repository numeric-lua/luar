-- jit/61_ir_float_arith.lua -- float subtraction and division specializations.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/61_ir_float_arith: skipped (no native target)")
  return
end

local t = {}
jit.off()
for i = 1, 100 do t[i] = i + 0.5 end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function sub (t, n)
  local s = 0
  for i = 1, n do s = s - t[i] end
  return s
end
local function div (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] / 2 end
  return s
end
H.eq(sub(t, 100), -5100.0)
H.eq(div(t, 100), 2550.0)
H.check(jit.stats().ir_executed >= 2, "float subtract/divide roots did not execute")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/61_ir_float_arith ok")
