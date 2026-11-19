-- jit/52_ir_diff.lua -- deterministic IR/side vs interpreter differential test.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/52_ir_diff: skipped (no native target)")
  return
end

local seed = 12345
local function rnd ()
  seed = (seed * 1103515245 + 12345) % 2147483648
  return seed
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function f (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] + i end
  return s
end

for round = 1, 20 do
  local n = 100 + rnd() % 500
  local t, h = {}, table.new(0, n)
  local expected = 0
  for i = 1, n do
    local v = i * 7 + round * 3
    t[i] = v
    h[i] = v
    expected = expected + v + i
  end
  H.eq(f(t, n), expected)
  H.eq(f(h, n), expected)
end

local st = jit.stats()
H.check(st.ir_compiled >= 1, "differential IR root was not compiled")
H.check(st.ir_side_compiled >= 1, "differential side was not compiled")
H.check(st.ir_side_executed >= 1, "differential side was not executed")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/52_ir_diff ok")
