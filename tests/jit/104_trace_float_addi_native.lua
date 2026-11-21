-- jit/104_trace_float_addi_native.lua -- FADDI now accumulates natively.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/104_trace_float_addi_native: skipped (no native target)")
  return
end

local v = 2.5
local function f_on (n)
  local s = 0.0
  for _ = 1, n do s = s + (v + 1) end
  return s
end
local function f_off (n)
  local s = 0.0
  for _ = 1, n do s = s + (v + 1) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(f_on(1000), 3500.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "FADDI loop did not compile")
H.check(st.ir_executed >= 1, "FADDI loop did not execute")
H.check(math.type(f_on(1)) == "float", "FADDI result is not a float")

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(f_off(1000), 3500.0)
print("jit/104_trace_float_addi_native ok")
