-- jit/90_trace_concatn.lua -- four+ operand concatenation in traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/90_trace_concatn: skipped (no native target)")
  return
end

local function f_on (n)
  local a, b, c, d = "a", "b", "c", "d"
  local last, s = "", 0
  for _ = 1, n do
    last = a .. b .. c .. d
    s = s + 1
  end
  return s, last
end
local function f_num (n)
  local a, b, c, d = 1, "x", 2.5, "!"
  local last, s = "", 0
  for _ = 1, n do
    last = a .. b .. c .. d
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, last = f_on(2000)
H.eq(s, 2000)
H.eq(last, "abcd")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "concatn trace did not compile")
H.check(st.ir_executed >= 1, "concatn trace did not execute")
collectgarbage()
collectgarbage()
H.eq(last, "abcd")

s, last = f_num(2000)
H.eq(s, 2000)
H.eq(last, "1x2.5!")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/90_trace_concatn ok")
