-- jit/109_trace_settabup_field.lua -- constant float writes through table
-- upvalues (SETTABUP).
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/109_trace_settabup_field: skipped (no native target)")
  return
end

local t = {}
local function f_on (n)
  for _ = 1, n do t.x = 2.5 end
  return t.x
end
local function f_off (n)
  for _ = 1, n do t.x = 2.5 end
  return t.x
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(f_on(2000), 2.5)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "SETTABUP field write did not compile")
H.check(st.ir_executed >= 1, "SETTABUP field write did not execute")
collectgarbage()
collectgarbage()
H.near(t.x, 2.5)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(f_off(2000), 2.5)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/109_trace_settabup_field ok")
