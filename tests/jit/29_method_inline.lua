-- jit/29_method_inline.lua -- inline one-field self:add() accumulator calls.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/29_method_inline: skipped (no native target)")
  return
end

local function run (n)
  local o = { v = 0 }
  function o:add (x)
    self.v = self.v + x
  end
  for i = 1, n do o:add(i) end
  return o.v
end

jit.flush()
jit.on()
H.eq(run(10000), 50005000)
H.eq(run(10000), 50005000)
H.check(jit.stats().compiled >= 1, "method loop did not compile")

-- upvalue table shape
local obj = { v = 0 }
function obj:add (x)
  self.v = self.v + x
end
local function urun (n)
  for i = 1, n do obj:add(i) end
  return obj.v
end
H.eq(urun(1000), 500500)
H.eq(urun(1000), 1001000)

-- other method bodies stay correct via fallback
local function run2 (n)
  local o = { v = 0 }
  function o:add (x)
    self.v = self.v + x * 2
  end
  for i = 1, n do o:add(i) end
  return o.v
end
H.eq(run2(10000), 100010000)

jit.off()
H.eq(run(100), 5050)
H.eq(run2(100), 10100)
jit.on()
print("jit/29_method_inline ok")
