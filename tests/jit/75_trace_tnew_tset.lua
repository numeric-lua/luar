-- jit/75_trace_tnew_tset.lua -- a traced empty table can be filled and read.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/75_trace_tnew_tset: skipped (no native target)")
  return
end

local function f (n)
  local s = 0
  for i = 1, n do
    local t = {}
    t[i] = i
    s = s + t[i]
  end
  return s
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
H.eq(f(2000), 2001000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "TNEW+TSET trace did not compile")
H.check(st.ir_executed >= 1, "TNEW+TSET trace did not execute")
collectgarbage()
collectgarbage()
H.eq(f(2000), 2001000)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f(2000), 2001000)
H.eq(jit.stats().ir_compiled, 0, "TNEW+TSET traced without alloc=1")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/75_trace_tnew_tset ok")
