-- jit/66_hash_insert.lua -- absent hash cells may be inserted by TSET.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/66_hash_insert: skipped (no native target)")
  return
end

local t = {}
local function f (t, n)
  local s = 0
  for i = 1001, n do
    t[i] = i
    s = s + t[i]
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f(t, 2000), (1001 + 2000) * 1000 / 2)
for i = 1001, 2000 do H.eq(rawget(t, i), i) end
H.check(jit.stats().ir_executed >= 1, "hash insertion root did not execute")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/66_hash_insert ok")
