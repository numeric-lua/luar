-- jit/108_trace_gettabup_field.lua -- field reads through table upvalues.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/108_trace_gettabup_field: skipped (no native target)")
  return
end

local t = { x = 2.5 }
local function f_on (n)
  local s = 0.0
  for _ = 1, n do s = s + t.x end
  return s
end
local function f_off (n)
  local s = 0.0
  for _ = 1, n do s = s + t.x end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(f_on(2000), 5000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "GETTABUP field read did not compile")
H.check(st.ir_executed >= 1, "GETTABUP field read did not execute")
collectgarbage()
collectgarbage()
H.near(f_on(2000), 5000.0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(f_off(2000), 5000.0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/108_trace_gettabup_field ok")
