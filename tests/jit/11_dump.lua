-- jit/11_dump.lua -- jit.dump toggle for trace/IR output.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/11_dump: skipped (no native target)")
  return
end

local function sum (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end

jit.dump("i")
H.eq(sum(1000), 500500)
jit.dump("t")
H.eq(sum(2000), 2001000)
jit.dump()
H.eq(sum(3000), 4501500)
H.check(jit.stats().compiled >= 1, "loop did not compile")

print("jit/11_dump ok")
