-- jit/27_closure_inline.lua -- inline a simple upvalue accumulator call.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/27_closure_inline: skipped (no native target)")
  return
end

local function run (n)
  local x = 0
  local f = function (v)
    x = x + v
    return x
  end
  for i = 1, n do f(i) end
  return x
end

jit.flush()
jit.on()
local first = run(10000)
H.eq(first, 50005000)
local second = run(10000)
H.eq(second, 50005000)
H.check(jit.stats().compiled >= 1, "closure loop did not compile")

-- a different closure body must stay correct via fallback
local function run2 (n)
  local x = 0
  local f = function (v)
    x = x + v * 2
    return x
  end
  for i = 1, n do f(i) end
  return x
end
H.eq(run2(10000), 100010000)

jit.off()
H.eq(run(100), 5050)
H.eq(run2(100), 10100)
jit.on()
print("jit/27_closure_inline ok")
