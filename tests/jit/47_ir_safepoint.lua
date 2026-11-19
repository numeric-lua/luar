-- jit/47_ir_safepoint.lua -- generated roots yield to the interpreter periodically.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/47_ir_safepoint: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function sum (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end

H.eq(sum(100000), 5000050000)
local st = jit.stats()
H.check(st.ir_executed >= 1, "IR root was not executed")
H.check(st.ir_yields >= 1, "IR root never yielded to the interpreter")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/47_ir_safepoint ok")
