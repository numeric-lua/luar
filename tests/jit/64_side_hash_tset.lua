-- jit/64_side_hash_tset.lua -- existing hash cells can be written by sides.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/64_side_hash_tset: skipped (no native target)")
  return
end

local t = {}
for i = 1001, 2000 do t[i] = 0 end
local function f (t, n)
  local s = 0
  for i = 1, n do
    t[1000 + i] = i
    s = s + t[1000 + i]
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f(t, 1000), 1000 * 1001 / 2)
for i = 1, 1000 do H.eq(t[1000 + i], i) end
H.check(jit.stats().ir_side_compiled >= 1, "hash TSET side not compiled")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/64_side_hash_tset ok")
