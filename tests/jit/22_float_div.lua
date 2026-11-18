-- jit/22_float_div.lua -- float division accumulator in a for-loop.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/22_float_div: skipped (no native target)")
  return
end

local function f (n)
  local s = 1.0
  for i = 1, n do s = s / i end
  return s, i
end

jit.flush()
jit.on()
local got, gi = f(100)
jit.off()
local want, wi = f(100)
H.eq(string.pack("d", got), string.pack("d", want))
H.eq(gi, wi)
H.check(got > 0.0 and got < 1e-100, "unexpected reciprocal product")
H.check(jit.stats().compiled >= 1, "float division did not compile")
print("jit/22_float_div ok")
