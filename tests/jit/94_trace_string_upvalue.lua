-- jit/94_trace_string_upvalue.lua -- guarded short-string upvalues.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/94_trace_string_upvalue: skipped (no native target)")
  return
end

local x = "a"
local function copy_loop (n)
  local y, s = "", 0
  for _ = 1, n do
    y = x
    s = s + 1
  end
  return s, y
end
local function concat_loop (n)
  local last, s = "", 0
  for _ = 1, n do
    last = x .. x
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, y = copy_loop(2000)
H.eq(s, 2000)
H.eq(y, "a")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "string upvalue trace did not compile")
H.check(st.ir_executed >= 1, "string upvalue trace did not execute")
s, y = concat_loop(2000)
H.eq(s, 2000)
H.eq(y, "aa")
collectgarbage()
collectgarbage()
H.eq(y, "aa")

-- A long string is outside the short-string tag and must fall back.
x = string.rep("x", 100)
local function long_loop (n)
  local last, s = "", 0
  for _ = 1, n do last = x; s = s + 1 end
  return s, last
end
s, y = long_loop(500)
H.eq(s, 500)
H.eq(#y, 100)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/94_trace_string_upvalue ok")
