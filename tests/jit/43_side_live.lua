-- jit/43_side_live.lua -- live side trace after an array-to-hash shape change.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/43_side_live: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("table=1")
jit.on()

local t = {}
for i = 1, 20000 do t[i] = i end
local function read (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end

H.eq(read(t, 20000), 400020000)

-- same keys, but now entirely in the hash part
local u = table.new(0, 20000)
for i = 1, 20000 do u[i] = i end
H.eq(read(u, 20000), 400020000)

local st = jit.stats()
H.check(st.ir_side_traces >= 1, "no side IR trace recorded")
H.check(st.ir_side_compiled >= 1, "side trace was not compiled")
H.check(st.ir_side_executed >= 1, "side trace was not executed")
H.check(st.ir_max_depth >= 1, "side-trace depth was not tracked")

jit.flush()
jit.opt.start("table=0")
jit.on()
print("jit/43_side_live ok")
