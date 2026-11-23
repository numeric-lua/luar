-- jit/35_midloop_exit.lua -- a table guard exits at the failing instruction PC.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
jit.opt.start("table=1", "tint=0")
if jit.status() ~= true then
  print("jit/35_midloop_exit: skipped (no native target)")
  return
end

local t = {}
for i = 1, 20000 do t[i] = i end
local function sum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end

H.eq(sum(t, 20000), 400020000)
local compiled = jit.stats().compiled
H.check(compiled >= 1, "table loop did not compile")

-- a late non-integer element forces a mid-loop guard exit
t[15000] = "bad"
local ok = pcall(function () return sum(t, 20000) end)
H.eq(ok, false)
H.check(jit.stats().last_exit_pc >= 0, "mid-loop exit PC was not recorded")
H.check(jit.stats().ir_side_traces >= 1, "no side-trace IR segment was recorded")
H.check(jit.stats().ir_side_ops >= 1, "side-trace IR segment is empty")
H.check(jit.stats().ir_max_depth >= 1, "side-trace depth was not tracked")

-- repair the table; the exact exit must not corrupt later iterations
t[15000] = 15000
H.eq(sum(t, 20000), 400020000)

jit.opt.start("table=0")
print("jit/35_midloop_exit ok")
