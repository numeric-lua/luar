-- jit/216_j4_sload_stale_tag.lua -- a SLOAD invalidates the destination's
-- recorded type, so a following ADD/K op must guard it and honor
-- metamethods.
--
-- The old trace kept seen[slot] from an earlier integer guard and emitted a
-- raw integer ADD/MULK with a table's address bits.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/216_j4_sload_stale_tag: skipped (no native target)")
  return
end

jit.opt.start("ir=1", "hotloop=1", "hotexit=1")

local add_calls, mul_calls = 0, 0
local T = setmetatable({}, {
  __add = function () add_calls = add_calls + 1; return 1000 end,
  __mul = function () mul_calls = mul_calls + 1; return 3 end,
})

local function add_loop (x, n)
  local s = 0
  local y = 0
  for i = 1, n do
    y = i
    s = s + y
    y = x
    s = s + y
    y = i
  end
  return s
end

local function mul_loop (x, n)
  local s = 1
  local y = 0
  for i = 1, n do
    y = i
    s = s + y
    y = x
    s = y * 2
    y = i
  end
  return s
end

for _ = 1, 3 do H.eq(add_loop(T, 200), 1000, "add metamethod") end
H.eq(add_calls, 600, "add call count")

for _ = 1, 3 do H.eq(mul_loop(T, 200), 3, "mul metamethod") end
H.eq(mul_calls, 600, "mul call count")

jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/216_j4_sload_stale_tag ok")
