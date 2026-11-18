-- jit/25_polymorphic.lua -- guard-exit driven numeric loop variant.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/25_polymorphic: skipped (no native target)")
  return
end

local function f (init, n)
  local s = init
  for i = 1, n do s = s + i end
  return s
end

jit.flush()
jit.on()
H.eq(f(0, 1000), 500500)
H.eq(f(0.0, 1000), 500500.0)
for _ = 1, 10 do
  H.eq(f(0, 1000), 500500)
  H.eq(f(0.0, 1000), 500500.0)
end
local st = jit.stats()
H.check(st.compiled >= 2, "guard-exit variant did not compile")
H.check(st.exits >= 1, "guard exits were not counted")

jit.off()
H.eq(f(0, 100), 5050)
H.eq(f(0.0, 100), 5050.0)
jit.on()
print("jit/25_polymorphic ok")
