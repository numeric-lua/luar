-- jit/115_trace_settabup_nil_field.lua -- nil deletion through table
-- upvalues.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/115_trace_settabup_nil_field: skipped (no native target)")
  return
end

local t = { x = 1 }
local function f_on (n)
  for _ = 1, n do t.x = nil end
  return rawget(t, "x")
end
local function f_off (n)
  for _ = 1, n do t.x = nil end
  return rawget(t, "x")
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f_on(2000), nil)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "SETTABUP nil write did not compile")
H.check(st.ir_executed >= 1, "SETTABUP nil write did not execute")
collectgarbage()
collectgarbage()
H.eq(rawget(t, "x"), nil)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), nil)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/115_trace_settabup_nil_field ok")
