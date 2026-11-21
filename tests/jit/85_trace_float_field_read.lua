-- jit/85_trace_float_field_read.lua -- float table-field reads in J4.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/85_trace_float_field_read: skipped (no native target)")
  return
end

local function f_on (n)
  local t = { x = 2.5 }
  local s = 0.0
  for _ = 1, n do s = s + t.x end
  return s
end
local function f_off (n)
  local t = { x = 2.5 }
  local s = 0.0
  for _ = 1, n do s = s + t.x end
  return s
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
H.near(f_on(2000), 5000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "float field read trace did not compile")
H.check(st.ir_executed >= 1, "float field read trace did not execute")
collectgarbage()
collectgarbage()
H.near(f_on(2000), 5000.0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(f_off(2000), 5000.0)
H.check(jit.stats().ir_executed >= 1,
        "float field read did not trace with alloc=0")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/85_trace_float_field_read ok")
