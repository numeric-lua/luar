-- jit/13_lin_sum.lua -- integer acc = acc + i * const fast path.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/13_lin_sum: skipped (no native target)")
  return
end

local function lin (n)
  local s = 0
  for i = 1, n do s = s + i * 3 end
  return s
end
local function linbig (n)
  local s = 0
  for i = 1, n do s = s + i * 1000000000000 end
  return s
end
H.eq(lin(100000), 15000150000)
H.eq(linbig(1000), 1000000000000 * 500500)
H.check(jit.stats().compiled >= 1, "linear loop did not compile")

jit.off()
H.eq(lin(1000), 1501500)
H.eq(linbig(100), 1000000000000 * 5050)
jit.on()

print("jit/13_lin_sum ok")
