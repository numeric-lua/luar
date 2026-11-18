-- jit/39_ir_execute.lua -- live execution of generated J4 root traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/39_ir_execute: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s = 0
for i = 1, 1000 do s = s + i end
H.eq(s, 500500)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "IR trace was not compiled")
H.check(st.ir_executed >= 1, "IR trace was not executed")

-- the opt-in path must leave legacy JIT compilation disabled
H.eq(st.compiled, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
local function fresh (n)
  local t = 0
  for i = 1, n do t = t + i end
  return t
end
H.eq(fresh(1000), 500500)
H.check(jit.stats().compiled >= 1, "legacy JIT did not resume")
print("jit/39_ir_execute ok")
