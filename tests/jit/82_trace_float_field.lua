-- jit/82_trace_float_field.lua -- dynamic float table field writes.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/82_trace_float_field: skipped (no native target)")
  return
end

local function f_on (n, x)
  local last
  local s = 0
  for _ = 1, n do
    last = { x = x }
    s = s + 1
  end
  return s, last
end
local function f_off (n, x)
  local last
  local s = 0
  for _ = 1, n do
    last = { x = x }
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, t = f_on(2000, 2.5)
H.eq(s, 2000)
H.near(t.x, 2.5)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "float field trace did not compile")
H.check(st.ir_executed >= 1, "float field trace did not execute")
collectgarbage()
collectgarbage()
s, t = f_on(2000, 3.25)
H.eq(s, 2000)
H.near(t.x, 3.25)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, t = f_off(2000, 2.5)
H.eq(s, 2000)
H.near(t.x, 2.5)
H.eq(jit.stats().ir_compiled, 0, "float field traced without alloc=1")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/82_trace_float_field ok")
