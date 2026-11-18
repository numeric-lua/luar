-- jit/23_table_intrinsic.lua -- raw fill/sum intrinsics are on by default.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.opt.start("tint=1")
jit.on()
if jit.status() ~= true then
  print("jit/23_table_intrinsic: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("tint=1")
jit.on()

local function fill (t, n)
  for i = 1, n do t[i] = i end
end
local function sum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s
end

local t = {}
fill(t, 5000)
H.eq(t[1], 1)
H.eq(t[5000], 5000)
H.eq(#t, 5000)
H.eq(sum(t, 5000), 12502500)

local f = {}
for i = 1, 1000 do f[i] = i + 0.5 end
H.eq(sum(f, 1000), 501000.0)
H.check(jit.stats().compiled >= 1, "table intrinsics did not compile")

jit.flush()
jit.off()
H.eq(sum(t, 5000), 12502500)
jit.on()
print("jit/23_table_intrinsic ok")
