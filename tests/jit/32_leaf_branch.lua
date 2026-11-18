-- jit/32_leaf_branch.lua -- inline branch-containing integer leaves.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/32_leaf_branch: skipped (no native target)")
  return
end

local function sum_call (f, n)
  local s = 0
  for i = 1, n do s = s + f(i) end
  return s
end

local function branch (x)
  if x > 5 then return x * 2 else return x + 1 end
end
local function abs (x)
  if x < 0 then return -x else return x end
end
local function both (x)
  if x < 5 then return x + 1 elseif x > 8 then return x * 2 else return x end
end

jit.flush()
jit.on()
H.eq(sum_call(branch, 1000), 1000990)
H.eq(sum_call(abs, 1000), 500500)
H.eq(sum_call(both, 1000), 1000968)
H.check(jit.stats().compiled >= 1, "branch leaf did not compile")

jit.off()
H.eq(sum_call(branch, 100), 10090)
H.eq(sum_call(abs, 100), 5050)
jit.on()
print("jit/32_leaf_branch ok")
