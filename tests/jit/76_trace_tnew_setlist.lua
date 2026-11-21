-- jit/76_trace_tnew_setlist.lua -- sized TNEW + SETLIST into TSETI.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/76_trace_tnew_setlist: skipped (no native target)")
  return
end

local function f_on (n)
  local t
  local s = 0
  for i = 1, n do
    t = {i}
    s = s + 1
  end
  return t[1], s
end
local function f_off (n)
  local t
  local s = 0
  for i = 1, n do
    t = {i}
    s = s + 1
  end
  return t[1], s
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local v, s = f_on(2000)
H.eq(v, 2000, "traced sized table has the wrong value")
H.eq(s, 2000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "TNEW+SETLIST trace did not compile")
H.check(st.ir_executed >= 1, "TNEW+SETLIST trace did not execute")

-- The per-iteration tables must survive collections and the trace must
-- remain valid.
collectgarbage()
collectgarbage()
v, s = f_on(2000)
H.eq(v, 2000, "traced sized table changed after GC")
H.eq(s, 2000)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
v, s = f_off(2000)
H.eq(v, 2000, "interpreter sized table has the wrong value")
H.eq(s, 2000)
H.eq(jit.stats().ir_compiled, 0, "TNEW+SETLIST traced without alloc=1")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/76_trace_tnew_setlist ok")
