-- jit/87_trace_concat.lua -- two-string concatenation in traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/87_trace_concat: skipped (no native target)")
  return
end

local function f_on (n)
  local a, b = "a", "b"
  local last, s = "", 0
  for _ = 1, n do
    last = a .. b
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local a, b = "a", "b"
  local last, s = "", 0
  for _ = 1, n do
    last = a .. b
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, last = f_on(2000)
H.eq(s, 2000)
H.eq(last, "ab")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "concat trace did not compile")
H.check(st.ir_executed >= 1, "concat trace did not execute")
collectgarbage()
collectgarbage()
H.eq(last, "ab")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, last = f_off(2000)
H.eq(s, 2000)
H.eq(last, "ab")
H.check(jit.stats().ir_executed >= 1, "concat did not trace with alloc=0")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/87_trace_concat ok")
