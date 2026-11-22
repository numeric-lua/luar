-- jit/125_trace_dynamic_int_value_nonempty.lua -- integer register values
-- on dynamic keys when the table is non-empty.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/125_trace_dynamic_int_value_nonempty: skipped (no native target)")
  return
end

local t = { seed = 1 }
local k = 2
local function f_on (n)
  local s = 0
  for _ = 1, n do
    t[k] = 7
    s = s + 1
  end
  return s, t[k]
end
local function f_off (n)
  local s = 0
  for _ = 1, n do
    t[k] = 7
    s = s + 1
  end
  return s, t[k]
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, v = f_on(2000)
H.eq(s, 2000)
H.eq(v, 7)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "non-empty dynamic int write did not compile")
H.check(st.ir_executed >= 1, "non-empty dynamic int write did not execute")
collectgarbage()
collectgarbage()
H.eq(t[k], 7)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, v = f_off(2000)
H.eq(s, 2000)
H.eq(v, 7)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/125_trace_dynamic_int_value_nonempty ok")
