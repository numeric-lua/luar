-- jit/77_trace_geti_seti.lua -- constant-index table access in J4.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/77_trace_geti_seti: skipped (no native target)")
  return
end

local function f_on (n)
  local s = 0
  for i = 1, n do
    local t = {}
    t[1] = i
    s = s + t[1]
  end
  return s
end
local function f_off (n)
  local s = 0
  for i = 1, n do
    local t = {}
    t[1] = i
    s = s + t[1]
  end
  return s
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
H.eq(f_on(2000), 2001000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "GETI/SETI trace did not compile")
H.check(st.ir_executed >= 1, "GETI/SETI trace did not execute")
collectgarbage()
collectgarbage()
H.eq(f_on(2000), 2001000)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), 2001000)
H.eq(jit.stats().ir_compiled, 0, "GETI/SETI traced without alloc=1")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/77_trace_geti_seti ok")
