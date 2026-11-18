-- jit/33_leaf_threearg.lua -- inline three-parameter integer leaves.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/33_leaf_threearg: skipped (no native target)")
  return
end

local function sum3 (f, n, b, c)
  local s = 0
  for i = 1, n do s = s + f(i, b, c) end
  return s
end

local function f (a, b, c) return a * b + c end
local function g (a, b, c) return a * b * c end
local function h (a, b, c) return a + b + c end
local function k (a, b, c) return a - b - c end
local function br (a, b, c)
  if a > b then return a + b - c else return c - a end
end

jit.flush()
jit.on()
H.eq(sum3(f, 10000, 2, 3), 100040000)
H.eq(sum3(g, 10000, 2, 3), 300030000)
H.eq(sum3(h, 10000, 2, 3), 50055000)
H.eq(sum3(k, 10000, 2, 3), 49955000)
H.eq(sum3(br, 1000, 500, 1000), 500000)
H.check(jit.stats().compiled >= 1, "three-arg leaf did not compile")

local b, c = 5, 7
H.eq(sum3(f, 1000, b, c), 2509500)
jit.off()
H.eq(sum3(f, 100, 2, 3), 10400)
H.eq(sum3(k, 100, 2, 3), 4550)
jit.on()
print("jit/33_leaf_threearg ok")
