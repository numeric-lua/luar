-- jit/110_trace_settabup_int_field.lua -- constant 64-bit integer writes
-- through table upvalues.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/110_trace_settabup_int_field: skipped (no native target)")
  return
end

local big = 1099511627777  -- 2^40 + 1
local t = {}
local function f_on (n)
  for _ = 1, n do t.x = 1099511627777 end
  return t.x
end
local function f_off (n)
  for _ = 1, n do t.x = 1099511627777 end
  return t.x
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f_on(2000), big)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "SETTABUP int write did not compile")
H.check(st.ir_executed >= 1, "SETTABUP int write did not execute")
collectgarbage()
collectgarbage()
H.eq(t.x, big)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), big)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/110_trace_settabup_int_field ok")
