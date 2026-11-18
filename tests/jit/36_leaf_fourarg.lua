-- jit/36_leaf_fourarg.lua -- inline four-parameter integer leaves.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/36_leaf_fourarg: skipped (no native target)")
  return
end

local function sum4 (f, n, b, c, d)
  local s = 0
  for i = 1, n do s = s + f(i, b, c, d) end
  return s
end

local function f (a, b, c, d) return a * b + c + d end
local function g (a, b, c, d) return a + b + c + d end
local function h (a, b, c, d) return a - b - c - d end
local function br (a, b, c, d)
  if a > b then return a + c - d else return d - a + c end
end

jit.flush()
jit.opt.start("hotloop=56")
jit.on()
H.eq(sum4(f, 1000, 2, 3, 4), 1008000)
H.eq(sum4(g, 1000, 2, 3, 4), 509500)
H.eq(sum4(h, 1000, 2, 3, 4), 491500)
H.eq(sum4(br, 1000, 500, 3, 4), 253000)
H.check(jit.stats().compiled >= 1, "four-arg leaf did not compile")

jit.off()
H.eq(sum4(f, 100, 2, 3, 4), 10800)
H.eq(sum4(g, 100, 2, 3, 4), 5950)
jit.on()
print("jit/36_leaf_fourarg ok")
