-- jit/88_trace_concat_numbers.lua -- number operands in traced concat.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/88_trace_concat_numbers: skipped (no native target)")
  return
end

local function f_on (n)
  local x, y = 1, "a"
  local last, s = "", 0
  for _ = 1, n do
    last = x .. y
    s = s + 1
  end
  return s, last
end
local function f_float (n)
  local x, y = 1.5, 2
  local last, s = "", 0
  for _ = 1, n do
    last = x .. y
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, last = f_on(2000)
H.eq(s, 2000)
H.eq(last, "1a")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "number concat trace did not compile")
H.check(st.ir_executed >= 1, "number concat trace did not execute")
collectgarbage()
collectgarbage()
H.eq(last, "1a")

s, last = f_float(2000)
H.eq(s, 2000)
H.eq(last, "1.52")
H.check(jit.stats().ir_executed >= 1, "float concat did not execute")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/88_trace_concat_numbers ok")
