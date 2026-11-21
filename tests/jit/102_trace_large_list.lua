-- jit/102_trace_large_list.lua -- SETLIST k/EXTRAARG beyond 50 elements.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/102_trace_large_list: skipped (no native target)")
  return
end

local function f_on (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60 }
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60 }
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, t = f_on(1000)
H.eq(s, 1000)
H.eq(t[1], 1)
H.eq(t[50], 50)
H.eq(t[60], 60)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "large list trace did not compile")
H.check(st.ir_executed >= 1, "large list trace did not execute")
collectgarbage()
collectgarbage()
H.eq(t[60], 60)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, t = f_off(1000)
H.eq(s, 1000)
H.eq(t[60], 60)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/102_trace_large_list ok")
