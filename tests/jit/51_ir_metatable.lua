-- jit/51_ir_metatable.lua -- table IR must not bypass __index.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/51_ir_metatable: skipped (no native target)")
  return
end

local t = {}
for i = 1, 100 do t[i] = i end
setmetatable(t, { __index = function (_, k) return k * 3 end })

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function sum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s
end

-- 1..100 from the array, 101..200 via __index
H.eq(sum(t, 200), 5050 + 3 * 15050)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/51_ir_metatable ok")
