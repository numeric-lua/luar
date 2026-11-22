-- jit/117_trace_dynamic_int_key_float.lua -- integer dynamic keys with
-- constant and register float values.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/117_trace_dynamic_int_key_float: skipped (no native target)")
  return
end

local t = {}
local k, v = 2, 2.5
local function f_on (n)
  local s = 0
  for _ = 1, n do
    t[k] = 3.5
    t[k] = v
    s = s + 1
  end
  return s, t[k]
end
local function f_off (n)
  local s = 0
  for _ = 1, n do
    t[k] = 3.5
    t[k] = v
    s = s + 1
  end
  return s, t[k]
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local s, x = f_on(2000)
H.eq(s, 2000)
H.near(x, 2.5)
H.check(math.type(x) == "float", "dynamic int-key float type lost")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "dynamic int-key float write did not compile")
H.check(st.ir_executed >= 1, "dynamic int-key float write did not execute")
collectgarbage()
collectgarbage()
H.near(t[k], 2.5)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, x = f_off(2000)
H.eq(s, 2000)
H.near(x, 2.5)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/117_trace_dynamic_int_key_float ok")
