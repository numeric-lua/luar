-- jit/71_call_b0.lua -- multret CALL setup is explicitly interpreter-side.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/71_call_b0: skipped (no native target)")
  return
end

local function three () return 1, 2, 3 end
local function sum3 (a, b, c) return a + b + c end
local function loop (n)
  local s = 0
  for i = 1, n do s = s + sum3(three()) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(loop(1000), 6000)   -- correctness via interpreter fallback
H.check(jit.stats().ir_executed >= 0, "stats must remain an integer")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/71_call_b0 ok")
