-- jit/119_trace_dynamic_key_int.lua -- constant integer values with
-- dynamic keys, preserving 64-bit values.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/119_trace_dynamic_key_int: skipped (no native target)")
  return
end

local t = {}
local k = "x"
local function f_on (n)
  local s = 0
  for _ = 1, n do
    t[k] = 1099511627777
    s = s + 1
  end
  return s, t[k]
end
local function f_off (n)
  local s = 0
  for _ = 1, n do
    t[k] = 1099511627777
    s = s + 1
  end
  return s, t[k]
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, v = f_on(2000)
H.eq(s, 2000)
H.eq(v, 1099511627777)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "dynamic-key int write did not compile")
H.check(st.ir_executed >= 1, "dynamic-key int write did not execute")
collectgarbage()
collectgarbage()
H.eq(t[k], 1099511627777)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, v = f_off(2000)
H.eq(s, 2000)
H.eq(v, 1099511627777)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/119_trace_dynamic_key_int ok")
