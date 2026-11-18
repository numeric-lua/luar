-- jit/30_leaf_inline.lua -- inline one-parameter leaf call results.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/30_leaf_inline: skipped (no native target)")
  return
end

local function sum_call (f, n)
  local s = 0
  for i = 1, n do s = s + f(i) end
  return s
end

local function twice (x) return x * 2 end
local function plus3 (x) return x + 3 end
local function square (x) return x * x end
local function dbl (x) return x + x end
local function other (x) return x * 2 + 1 end

jit.flush()
jit.on()
H.eq(sum_call(twice, 10000), 100010000)
H.eq(sum_call(plus3, 10000), 50035000)
H.eq(sum_call(square, 10000), 333383335000)
H.eq(sum_call(dbl, 10000), 100010000)
H.eq(sum_call(other, 1000), 1002000)
H.check(jit.stats().compiled >= 1, "leaf call did not compile")

jit.off()
H.eq(sum_call(twice, 100), 10100)
H.eq(sum_call(square, 100), 338350)
jit.on()
print("jit/30_leaf_inline ok")
