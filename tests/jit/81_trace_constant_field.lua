-- jit/81_trace_constant_field.lua -- constant integer table field writes.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/81_trace_constant_field: skipped (no native target)")
  return
end

local function f_on (n)
  local s = 0
  for _ = 1, n do
    local t = { x = 1 }
    s = s + t.x
  end
  return s
end
local function f_off (n)
  local s = 0
  for _ = 1, n do
    local t = { x = 1 }
    s = s + t.x
  end
  return s
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
H.eq(f_on(2000), 2000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "constant field trace did not compile")
H.check(st.ir_executed >= 1, "constant field trace did not execute")
collectgarbage()
collectgarbage()
H.eq(f_on(2000), 2000)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), 2000)
H.eq(jit.stats().ir_compiled, 0, "constant field traced without alloc=1")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/81_trace_constant_field ok")
