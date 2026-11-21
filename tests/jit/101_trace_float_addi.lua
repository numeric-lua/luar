-- jit/101_trace_float_addi.lua -- float source + integer immediate must not
-- bit-add the float payload.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/101_trace_float_addi: skipped (no native target)")
  return
end

local t = {}
local ki, v = 2, 2.5

local function f (n)
  local s = 0
  for _ = 1, n do
    t[ki] = v + 1
    s = s + 1
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f(2000), 2000)
H.near(t[2], 3.5)
H.check(math.type(t[2]) == "float", "float add immediate lost its type")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/101_trace_float_addi ok")
