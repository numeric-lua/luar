-- jit/121_trace_dynamic_key_table.lua -- table values with dynamic keys.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/121_trace_dynamic_key_table: skipped (no native target)")
  return
end

local inner = { v = 9 }
local t = {}
local k = "x"
local function f_on (n)
  local s = 0
  for _ = 1, n do
    t[k] = inner
    s = s + 1
  end
  return s, t[k]
end
local function f_off (n)
  local s = 0
  for _ = 1, n do
    t[k] = inner
    s = s + 1
  end
  return s, t[k]
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, v = f_on(2000)
H.eq(s, 2000)
H.check(rawequal(v, inner), "dynamic-key table identity lost")
H.eq(v.v, 9)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "dynamic-key table write did not compile")
H.check(st.ir_executed >= 1, "dynamic-key table write did not execute")
collectgarbage()
collectgarbage()
H.check(rawequal(t[k], inner), "dynamic-key table did not survive GC")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, v = f_off(2000)
H.eq(s, 2000)
H.check(rawequal(v, inner))

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/121_trace_dynamic_key_table ok")
