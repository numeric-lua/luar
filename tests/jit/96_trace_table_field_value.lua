-- jit/96_trace_table_field_value.lua -- table-valued field writes.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/96_trace_table_field_value: skipped (no native target)")
  return
end

local inner = { x = 2.5 }
local function f_on (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { x = inner }
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { x = inner }
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, t = f_on(2000)
H.eq(s, 2000)
H.check(rawequal(t.x, inner), "field did not reference the same table")
H.near(t.x.x, 2.5)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "table-valued field trace did not compile")
H.check(st.ir_executed >= 1, "table-valued field trace did not execute")
collectgarbage()
collectgarbage()
H.check(rawequal(t.x, inner), "field table did not survive GC")
H.near(t.x.x, 2.5)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, t = f_off(2000)
H.eq(s, 2000)
H.check(rawequal(t.x, inner))
H.near(t.x.x, 2.5)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/96_trace_table_field_value ok")
