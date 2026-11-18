-- jit/21_float_sum.lua -- float accumulator in a numeric for-loop.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/21_float_sum: skipped (no native target)")
  return
end

local function f (n)
  local s = 0.0
  for i = 1, n do s = s + i end
  return s, i
end

jit.flush()
jit.on()
local got, gi = f(100000)
jit.off()
local want, wi = f(100000)
H.eq(got, want)
H.eq(gi, wi)
H.check(jit.stats().compiled >= 1, "float sum did not compile")

jit.off()
H.eq(f(100), 5050.0)
jit.on()
print("jit/21_float_sum ok")
