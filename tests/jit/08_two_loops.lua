-- jit/08_two_loops.lua -- more than one native loop per Proto.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/08_two_loops: skipped (no native target)")
  return
end

local function two (n)
  local a = 0
  for i = 1, n do a = a + i end
  local b = 0.0
  for i = 1, n do b = b + i * 0.5 end
  return a, b
end

local a, b = two(100000)
H.eq(a, 5000050000)
H.eq(b, 2500025000.0)
H.check(jit.stats().compiled >= 2, "expected two native loops, got " ..
        tostring(jit.stats().compiled))

-- a third loop replaces the second slot but stays correct
local function three (n)
  local a, b, c = 0, 0.0, 0
  for i = 1, n do a = a + i end
  for i = 1, n do b = b + i * 0.25 end
  for i = 1, n do c = c + i end
  return a, b, c
end
jit.flush()
jit.on()
local x, y, z = three(50000)
H.eq(x, 1250025000)
H.eq(y, 312506250.0)
H.eq(z, 1250025000)

jit.off()
H.eq(two(1000), 500500)
jit.on()

print("jit/08_two_loops ok")
