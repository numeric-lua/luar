-- jit/208_j4_tsetkk_i64.lua -- constant integer SETFIELD values keep all
-- 64 bits in J4.
--
-- TSETKK stored the value in a double, so constants above 2^53 rounded.
-- The IR now carries the raw integer bits, matching the interpreter for
-- positive, negative and boundary values.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/208_j4_tsetkk_i64: skipped (no native target)")
  return
end

local function same (a, b)
  return math.type(a) == math.type(b) and a == b
end

local function field (value, n)
  local t = {}
  for _ = 1, n do t.x = value end
  return t.x, math.type(t.x)
end

local function run (value, n)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.off()
  local want, wt = field(value, n)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.on()
  local got, gt = field(value, n)
  H.check(same(got, want), tostring(value) .. ": " .. tostring(got) ..
          " ~= " .. tostring(want))
  H.eq(gt, wt, tostring(value) .. ": type")
end

run(9007199254740993, 200)              -- 2^53 + 1
run(-9007199254740993, 200)
run(9223372036854775807, 200)           -- maxinteger
run(-9223372036854775807 - 1, 200)      -- mininteger
run(12345678901234567, 200)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/208_j4_tsetkk_i64 ok")
