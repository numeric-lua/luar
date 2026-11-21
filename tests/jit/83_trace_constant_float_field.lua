-- jit/83_trace_constant_float_field.lua -- constant float field writes.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/83_trace_constant_float_field: skipped (no native target)")
  return
end

local function f_on (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { x = 2.5 }
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local last, s = nil, 0
  for _ = 1, n do
    last = { x = 2.5 }
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, t = f_on(2000)
H.eq(s, 2000)
H.near(t.x, 2.5)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "constant float field trace did not compile")
H.check(st.ir_executed >= 1, "constant float field trace did not execute")
collectgarbage()
collectgarbage()
H.eq(t.x, 2.5)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, t = f_off(2000)
H.eq(s, 2000)
H.near(t.x, 2.5)
H.eq(jit.stats().ir_compiled, 0, "constant float field traced without alloc=1")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/83_trace_constant_float_field ok")
