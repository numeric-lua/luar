-- jit/113_trace_settabup_table_field.lua -- table-valued writes through
-- table upvalues.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/113_trace_settabup_table_field: skipped (no native target)")
  return
end

local inner = { v = 9 }
local t = {}
local function f_on (n)
  for _ = 1, n do t.x = inner end
  return t.x
end
local function f_off (n)
  for _ = 1, n do t.x = inner end
  return t.x
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local v = f_on(2000)
H.check(rawequal(v, inner), "SETTABUP table write lost identity")
H.eq(v.v, 9)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "SETTABUP table write did not compile")
H.check(st.ir_executed >= 1, "SETTABUP table write did not execute")
collectgarbage()
collectgarbage()
H.check(rawequal(t.x, inner), "SETTABUP table write did not survive GC")
H.eq(t.x.v, 9)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
v = f_off(2000)
H.check(rawequal(v, inner))

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/113_trace_settabup_table_field ok")
