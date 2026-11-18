-- jit/26_hotexit.lua -- guard exits accumulate before a variant is recorded.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/26_hotexit: skipped (no native target)")
  return
end

local function f (init, n)
  local s = init
  for i = 1, n do s = s + i end
  return s
end

jit.flush()
jit.opt.start("hotexit=3")
jit.on()
H.eq(f(0, 1000), 500500)
local first = jit.stats().compiled
H.eq(f(0.0, 1000), 500500.0)
H.check(jit.stats().exits >= 3, "hotexit guard failures were not counted")
H.check(jit.stats().compiled > first, "variant did not compile after hotexit")
H.eq(f(0, 1000), 500500)
H.eq(f(0.0, 1000), 500500.0)

jit.flush()
jit.opt.start("hotexit=1000000")
jit.on()
local function g (init, n)
  local s = init
  for i = 1, n do s = s + i end
  return s
end
H.eq(g(0, 100), 5050)
H.eq(g(0.0, 100), 5050.0)
H.eq(jit.stats().compiled, 1, "hotexit=1000000 compiled a variant too early")

jit.opt.start("hotexit=10")
print("jit/26_hotexit ok")
