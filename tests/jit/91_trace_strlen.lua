-- jit/91_trace_strlen.lua -- string length in traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/91_trace_strlen: skipped (no native target)")
  return
end

local function f_on (n)
  local s = "hello"
  local x = 0
  for _ = 1, n do x = x + #s end
  return x
end
local function f_off (n)
  local s = "hello"
  local x = 0
  for _ = 1, n do x = x + #s end
  return x
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f_on(2000), 10000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "strlen trace did not compile")
H.check(st.ir_executed >= 1, "strlen trace did not execute")
collectgarbage()
collectgarbage()
H.eq(f_on(2000), 10000)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), 10000)
H.check(jit.stats().ir_executed >= 1, "strlen did not trace with alloc=0")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/91_trace_strlen ok")
