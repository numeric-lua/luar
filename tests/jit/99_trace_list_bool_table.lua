-- jit/99_trace_list_bool_table.lua -- boolean/table list elements.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/99_trace_list_bool_table: skipped (no native target)")
  return
end

local inner = { x = 9 }
local function f_on (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { true, inner, "x", 1, 2.5 }
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { true, inner, "x", 1, 2.5 }
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, t = f_on(2000)
H.eq(s, 2000)
H.eq(t[1], true)
H.check(rawequal(t[2], inner), "list table element identity lost")
H.near(t[2].x, 9)
H.eq(t[3], "x")
H.eq(t[4], 1)
H.near(t[5], 2.5)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "bool/table list trace did not compile")
H.check(st.ir_executed >= 1, "bool/table list trace did not execute")
collectgarbage()
collectgarbage()
H.check(rawequal(t[2], inner), "list table element did not survive GC")
H.near(t[2].x, 9)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, t = f_off(2000)
H.eq(s, 2000)
H.eq(t[1], true)
H.check(rawequal(t[2], inner))
H.eq(t[3], "x")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/99_trace_list_bool_table ok")
