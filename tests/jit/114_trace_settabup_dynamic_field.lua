-- jit/114_trace_settabup_dynamic_field.lua -- register writes through table
-- upvalues (TSETUPKV).
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/114_trace_settabup_dynamic_field: skipped (no native target)")
  return
end

local fv, sv = 2.5, "hi"
local t = {}
local function f_on (n)
  for _ = 1, n do
    t.x = fv
    t.y = sv
  end
  return t.x, t.y
end
local function f_off (n)
  for _ = 1, n do
    t.x = fv
    t.y = sv
  end
  return t.x, t.y
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local x, y = f_on(2000)
H.near(x, 2.5)
H.eq(y, "hi")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "dynamic SETTABUP write did not compile")
H.check(st.ir_executed >= 1, "dynamic SETTABUP write did not execute")
collectgarbage()
collectgarbage()
H.near(t.x, 2.5)
H.eq(t.y, "hi")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
x, y = f_off(2000)
H.near(x, 2.5)
H.eq(y, "hi")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/114_trace_settabup_dynamic_field ok")
