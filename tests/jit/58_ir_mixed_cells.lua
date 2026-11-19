-- jit/58_ir_mixed_cells.lua -- int/float cell changes must not corrupt roots.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/58_ir_mixed_cells: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

local ti, tf = {}, {}
jit.off()
for i = 1, 1000 do ti[i] = i; tf[i] = i + 0.5 end
jit.on()

local function sum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end

H.eq(sum(ti, 1000), 1001000)
ti[500] = 500.5
H.eq(sum(ti, 1000), 1001001.0)
ti[500] = 500
H.eq(sum(ti, 1000), 1001000)

-- same Proto specialized on a float table, then retried with ints
local function fsum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end
H.eq(fsum(tf, 1000), 1002000.0)
H.eq(fsum(ti, 1000), 1001000)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/58_ir_mixed_cells ok")
