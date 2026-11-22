-- jit/111_trace_settabup_string_field.lua -- constant string writes through
-- table upvalues.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/111_trace_settabup_string_field: skipped (no native target)")
  return
end

local t = {}
local function f_on (n)
  for _ = 1, n do t.x = "hello" end
  return t.x
end
local function f_off (n)
  for _ = 1, n do t.x = "hello" end
  return t.x
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f_on(2000), "hello")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "SETTABUP string write did not compile")
H.check(st.ir_executed >= 1, "SETTABUP string write did not execute")
collectgarbage()
collectgarbage()
H.eq(t.x, "hello")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), "hello")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/111_trace_settabup_string_field ok")
