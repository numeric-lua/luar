-- jit/63_side_tset.lua -- side segments may execute TSET once, exactly.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/63_side_tset: skipped (no native target)")
  return
end

local function f0 (x) return x + 1 end
local function f1 (x) return x + 2 end
local t = {}
for i = 1, 1000 do t[i] = 0 end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function g (f, t, n)
  local s = 0
  for i = 1, n do
    s = s + f(i)   -- CGUARD changes between calls
    t[i] = i       -- child/side must perform this write
  end
  return s
end

H.eq(g(f0, t, 1000), 1000 * 1001 / 2 + 1000)
H.eq(g(f1, t, 1000), 1000 * 1001 / 2 + 2000)
for i = 1, 1000 do H.eq(t[i], i) end
H.check(jit.stats().ir_side_compiled >= 1, "TSET side not compiled")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/63_side_tset ok")
