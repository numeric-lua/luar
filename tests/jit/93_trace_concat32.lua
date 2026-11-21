-- jit/93_trace_concat32.lua -- ten-operand concatenation in traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/93_trace_concat32: skipped (no native target)")
  return
end

local function f_on (n)
  local a, b, c, d, e = "a", "b", "c", "d", "e"
  local f, g, h, i, j = "f", "g", "h", "i", "j"
  local last, s = "", 0
  for _ = 1, n do
    last = a .. b .. c .. d .. e .. f .. g .. h .. i .. j
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local a, b, c, d, e = "a", "b", "c", "d", "e"
  local f, g, h, i, j = "f", "g", "h", "i", "j"
  local last, s = "", 0
  for _ = 1, n do
    last = a .. b .. c .. d .. e .. f .. g .. h .. i .. j
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, last = f_on(2000)
H.eq(s, 2000)
H.eq(last, "abcdefghij")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "ten-operand concat trace did not compile")
H.check(st.ir_executed >= 1, "ten-operand concat trace did not execute")
collectgarbage()
collectgarbage()
H.eq(last, "abcdefghij")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, last = f_off(2000)
H.eq(s, 2000)
H.eq(last, "abcdefghij")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/93_trace_concat32 ok")
