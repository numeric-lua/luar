-- jit/112_trace_settabup_bool_field.lua -- constant boolean writes through
-- table upvalues.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/112_trace_settabup_bool_field: skipped (no native target)")
  return
end

local t = {}
local function f_on (n)
  for _ = 1, n do t.x = true end
  return t.x
end
local function f_off (n)
  for _ = 1, n do t.x = true end
  return t.x
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f_on(2000), true)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "SETTABUP bool write did not compile")
H.check(st.ir_executed >= 1, "SETTABUP bool write did not execute")
collectgarbage()
collectgarbage()
H.eq(t.x, true)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), true)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/112_trace_settabup_bool_field ok")
