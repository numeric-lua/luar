-- jit/31_leaf_twoarg.lua -- inline two-parameter straight-line leaves.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/31_leaf_twoarg: skipped (no native target)")
  return
end

local function sum2 (f, n, b)
  local s = 0
  for i = 1, n do s = s + f(i, b) end
  return s
end

local function f (a, b) return a * b + b end
local function g (a, b) return a - b end
local function h (a, b) return a * b end
local function bad (a, b) return a * b + 1 end

jit.flush()
jit.on()
H.eq(sum2(f, 10000, 2), 100030000)
H.eq(sum2(g, 10000, 3), 49975000)
H.eq(sum2(h, 10000, 4), 200020000)
H.eq(sum2(bad, 1000, 2), 1002000)
H.check(jit.stats().compiled >= 1, "two-arg leaf did not compile")

local b = 5
H.eq(sum2(f, 1000, b), 2507500)
jit.off()
H.eq(sum2(f, 100, 2), 10300)
H.eq(sum2(g, 100, 3), 4750)
jit.on()
print("jit/31_leaf_twoarg ok")
