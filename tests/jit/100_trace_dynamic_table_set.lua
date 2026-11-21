-- jit/100_trace_dynamic_table_set.lua -- upvalue-loaded string-key writes.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/100_trace_dynamic_table_set: skipped (no native target)")
  return
end

local t = {}
local k, v = "x", 2.5
local function f_on (n)
  local s = 0
  for _ = 1, n do
    t[k] = v
    s = s + 1
  end
  return s
end
local function f_off (n)
  local s = 0
  for _ = 1, n do
    t[k] = v
    s = s + 1
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f_on(2000), 2000)
H.near(t.x, 2.5)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "dynamic table set trace did not compile")
H.check(st.ir_executed >= 1, "dynamic table set trace did not execute")
collectgarbage()
collectgarbage()
H.near(t.x, 2.5)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), 2000)
H.near(t.x, 2.5)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/100_trace_dynamic_table_set ok")
