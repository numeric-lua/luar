-- jit/19_guard_exits.lua -- guard-failure counter (side-exit groundwork).
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/19_guard_exits: skipped (no native target)")
  return
end

local function sum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s
end

jit.flush()
jit.opt.start("table=1")
jit.on()
local ints = {}
local floats = {}
for i = 1, 200 do
  ints[i] = i
  floats[i] = i + 0.5
end
H.eq(sum(ints, 200), 20100)
local before = jit.stats().exits
H.eq(sum(floats, 200) ~= nil, true)
H.check(jit.stats().exits > before, "guard failures were not counted")

jit.flush()
H.eq(jit.stats().exits, 0, "flush must reset the exit counter")
jit.opt.start("table=0")
print("jit/19_guard_exits ok")
