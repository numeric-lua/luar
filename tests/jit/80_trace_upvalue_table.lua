-- jit/80_trace_upvalue_table.lua -- a closed-over table can be the target
-- of traced TSET/TGET operations.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/80_trace_upvalue_table: skipped (no native target)")
  return
end

local t = {}
local function fill (n)
  local s = 0
  for i = 1, n do
    t[i] = i
    s = s + t[i]
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(fill(2000), 2001000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "table upvalue trace did not compile")
H.check(st.ir_executed >= 1, "table upvalue trace did not execute")
collectgarbage()
collectgarbage()
H.eq(t[1], 1)
H.eq(t[2000], 2000)

-- A different table upvalue must not reuse the first table's trace.
local t2 = {}
local function fill2 (n)
  local s = 0
  for i = 1, n do
    t2[i] = i + 1
    s = s + t2[i]
  end
  return s
end
H.eq(fill2(1000), 501500)
H.eq(t2[1000], 1001)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/80_trace_upvalue_table ok")
