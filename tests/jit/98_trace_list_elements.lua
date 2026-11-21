-- jit/98_trace_list_elements.lua -- mixed float/string list elements.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/98_trace_list_elements: skipped (no native target)")
  return
end

local function f_on (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { 1, 2.5, "x" }
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { 1, 2.5, "x" }
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, t = f_on(2000)
H.eq(s, 2000)
H.eq(t[1], 1)
H.near(t[2], 2.5)
H.eq(t[3], "x")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "mixed list trace did not compile")
H.check(st.ir_executed >= 1, "mixed list trace did not execute")
collectgarbage()
collectgarbage()
H.eq(t[1], 1)
H.near(t[2], 2.5)
H.eq(t[3], "x")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, t = f_off(2000)
H.eq(s, 2000)
H.eq(t[1], 1)
H.near(t[2], 2.5)
H.eq(t[3], "x")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/98_trace_list_elements ok")
