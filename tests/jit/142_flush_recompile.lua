-- jit/142_flush_recompile.lua -- jit.flush() invalidates the per-Proto hot
-- counter, so a flushed loop can become hot and compile again.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/142_flush_recompile: skipped (no native target)")
  return
end

jit.flush()
jit.on()

local function f (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end

H.eq(f(10000), 50005000)
H.check(jit.stats().compiled >= 1, "loop was not compiled before flush")

jit.flush()
H.eq(jit.stats().compiled, 0, "jit.flush left compiled code counted")
H.eq(f(10000), 50005000)
H.check(jit.stats().compiled >= 1, "flushed loop did not recompile")

-- A second flush must behave the same way.
jit.flush()
H.eq(f(5000), 12502500)
H.check(jit.stats().compiled >= 1, "loop did not recompile after a second flush")

print("jit/142_flush_recompile ok")
