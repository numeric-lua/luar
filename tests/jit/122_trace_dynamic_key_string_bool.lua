-- jit/122_trace_dynamic_key_string_bool.lua -- string/boolean register
-- values on dynamic keys.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/122_trace_dynamic_key_string_bool: skipped (no native target)")
  return
end

local t = {}
local k, s, b = "x", "hi", true
local function f_on (n)
  local c = 0
  for _ = 1, n do
    t[k] = s
    t[k] = b
    c = c + 1
  end
  return c, t[k]
end
local function f_off (n)
  local c = 0
  for _ = 1, n do
    t[k] = s
    t[k] = b
    c = c + 1
  end
  return c, t[k]
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local c, v = f_on(2000)
H.eq(c, 2000)
H.eq(v, true)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "dynamic-key string/bool write did not compile")
H.check(st.ir_executed >= 1, "dynamic-key string/bool write did not execute")
collectgarbage()
collectgarbage()
H.eq(t[k], true)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
c, v = f_off(2000)
H.eq(c, 2000)
H.eq(v, true)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/122_trace_dynamic_key_string_bool ok")
