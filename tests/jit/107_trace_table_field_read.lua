-- jit/107_trace_table_field_read.lua -- table-valued field reads.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/107_trace_table_field_read: skipped (no native target)")
  return
end

local inner = { v = 7 }
local function f_on (n)
  local t = { x = inner }
  local last, s = nil, 0
  for _ = 1, n do
    last = t.x
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local t = { x = inner }
  local last, s = nil, 0
  for _ = 1, n do
    last = t.x
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, t = f_on(2000)
H.eq(s, 2000)
H.check(rawequal(t, inner), "field table identity lost")
H.eq(t.v, 7)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "table field read did not compile")
H.check(st.ir_executed >= 1, "table field read did not execute")
collectgarbage()
collectgarbage()
H.check(rawequal(t, inner), "field table did not survive GC")
H.eq(t.v, 7)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, t = f_off(2000)
H.eq(s, 2000)
H.check(rawequal(t, inner))
H.eq(t.v, 7)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/107_trace_table_field_read ok")
