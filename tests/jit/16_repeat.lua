-- jit/16_repeat.lua -- repeat/until conditional backedges.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/16_repeat: skipped (no native target)")
  return
end

local function roff (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i until i >= n
  return s, i
end
local function ron (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i until i >= n
  return s, i
end
local function rstep (n)
  local i, s = 0, 0
  repeat i = i + 2; s = s + i until i >= n
  return s, i
end

jit.off()
local want, wi = roff(100000)
local wstep = rstep(100000)
jit.on()
local got, gi = ron(100000)
H.eq(got, want)
H.eq(gi, wi)
H.eq(rstep(100000), wstep)
H.check(jit.stats().compiled >= 1, "repeat loop did not compile")
H.check(jit.stats().traces >= 1, "repeat trace missing")

jit.off()
H.eq(ron(1000), roff(1000))
H.eq(rstep(1000), 250500)
jit.on()

print("jit/16_repeat ok")
