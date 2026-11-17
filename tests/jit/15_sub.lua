-- jit/15_sub.lua -- register-cached subtraction loops.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/15_sub: skipped (no native target)")
  return
end

local function dsub (n)
  local s = 0
  for i = 1, n do s = s - i end
  return s
end
local function dfsub (n)
  local s = 0.0
  for i = 1, n do s = s - i end
  return s
end
H.eq(dsub(100000), -5000050000)
H.eq(dfsub(100000), -5000050000.0)
H.check(jit.stats().compiled >= 1, "subtraction loop did not compile")

jit.off()
H.eq(dsub(1000), -500500)
H.eq(dfsub(1000), -500500.0)
jit.on()

print("jit/15_sub ok")
