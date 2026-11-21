-- jit/89_trace_concat3.lua -- three-operand concatenation in traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/89_trace_concat3: skipped (no native target)")
  return
end

local function f_on (n)
  local a, b, c = "a", "b", "c"
  local last, s = "", 0
  for _ = 1, n do
    last = a .. b .. c
    s = s + 1
  end
  return s, last
end
local function f_num (n)
  local a, b, c = 1, "x", 2.5
  local last, s = "", 0
  for _ = 1, n do
    last = a .. b .. c
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, last = f_on(2000)
H.eq(s, 2000)
H.eq(last, "abc")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "three-way concat trace did not compile")
H.check(st.ir_executed >= 1, "three-way concat trace did not execute")
collectgarbage()
collectgarbage()
H.eq(last, "abc")

s, last = f_num(2000)
H.eq(s, 2000)
H.eq(last, "1x2.5")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/89_trace_concat3 ok")
