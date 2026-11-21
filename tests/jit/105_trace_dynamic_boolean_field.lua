-- jit/105_trace_dynamic_boolean_field.lua -- dynamic boolean field writes.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/105_trace_dynamic_boolean_field: skipped (no native target)")
  return
end

local function f_on (n, b)
  local last, s = nil, 0
  for _ = 1, n do
    last = { x = b }
    s = s + 1
  end
  return s, last
end
local function f_off (n, b)
  local last, s = nil, 0
  for _ = 1, n do
    last = { x = b }
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, t = f_on(2000, true)
H.eq(s, 2000)
H.eq(t.x, true)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "dynamic bool field trace did not compile")
H.check(st.ir_executed >= 1, "dynamic bool field trace did not execute")
s, t = f_on(1, false)
H.eq(t.x, false)
collectgarbage()
collectgarbage()
H.eq(t.x, false)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, t = f_off(2000, true)
H.eq(s, 2000)
H.eq(t.x, true)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/105_trace_dynamic_boolean_field ok")
