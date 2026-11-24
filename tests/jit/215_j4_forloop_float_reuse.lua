-- jit/215_j4_forloop_float_reuse.lua -- J4 integer FORLOOP traces must not
-- be entered for a float-controlled loop that reaches the same pc/base.
--
-- The control triplet is re-checked at IR entry.  The old trace used the
-- float payloads as raw integers, producing a wrong trip count or a hang.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/215_j4_forloop_float_reuse: skipped (no native target)")
  return
end

jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
local function f (init, lim, step)
  local n = 0
  for _ = init, lim, step do n = n + 1 end
  return n
end

for _ = 1, 5 do H.eq(f(1, 100, 1), 100, "integer warmup") end
H.eq(f(0.0, 5e-324, 5e-324), 2, "tiny float step")
H.eq(f(1.5, 5.5, 1.0), 5, "float limits")
H.eq(f(1, 100, 1), 100, "integer again")

jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/215_j4_forloop_float_reuse ok")
