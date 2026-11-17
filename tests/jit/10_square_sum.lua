-- jit/10_square_sum.lua -- register-cached sum of squares.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/10_square_sum: skipped (no native target)")
  return
end

local function sq (n)
  local s = 0
  for i = 1, n do s = s + i * i end
  return s
end
H.eq(sq(100000), 333338333350000)
H.check(jit.stats().compiled >= 1, "square loop did not compile")

local function sqstep (n)
  local s = 0
  for i = 1, n, 2 do s = s + i * i end
  return s
end
jit.off()
local want = sqstep(10000)
jit.on()
H.eq(sqstep(10000), want)

jit.off()
H.eq(sq(1000), 333833500)
H.eq(sqstep(10000), want)
jit.on()

print("jit/10_square_sum ok")
