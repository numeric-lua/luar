-- jit/74_trace_tnew.lua -- opt-in empty-table allocation in J4.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/74_trace_tnew: skipped (no native target)")
  return
end

local function f_off (n)
  local s = 0
  for _ = 1, n do
    local t = {}
    s = s + 1
  end
  return s
end
local function f_on (n)
  local s = 0
  for _ = 1, n do
    local t = {}
    s = s + 1
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f_off(1000), 1000)
H.eq(jit.stats().ir_compiled, 0, "TNEW traced without alloc=1")

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
H.eq(f_on(1000), 1000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "TNEW trace did not compile")
H.check(st.ir_executed >= 1, "TNEW trace did not execute")
collectgarbage()
collectgarbage()
H.eq(f_on(1000), 1000)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(1000), 1000)
H.eq(jit.stats().ir_compiled, 0, "TNEW traced with alloc=0")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/74_trace_tnew ok")
