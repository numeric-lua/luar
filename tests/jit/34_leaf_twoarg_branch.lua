-- jit/34_leaf_twoarg_branch.lua -- branchy two-parameter leaves.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/34_leaf_twoarg_branch: skipped (no native target)")
  return
end

local function sum2 (f, n, b)
  local s = 0
  for i = 1, n do s = s + f(i, b) end
  return s
end

local function absdiff (a, b)
  if a > b then return a - b else return b - a end
end
local function pick (a, b)
  if a < b then return a * 2 else return b * 2 end
end
local function outer (a, b)
  if a % 2 == 0 then return a + b elseif a > b then return a - b else return a * b end
end

jit.flush()
jit.on()
H.eq(sum2(absdiff, 10000, 5000), 25000000)
H.eq(sum2(pick, 1000, 300), 510300)
H.eq(sum2(outer, 1000, 7), 500624)
H.check(jit.stats().compiled >= 1, "branchy two-arg leaf did not compile")

jit.off()
H.eq(sum2(absdiff, 100, 50), 2500)
H.eq(sum2(pick, 100, 30), 5130)
jit.on()
print("jit/34_leaf_twoarg_branch ok")
