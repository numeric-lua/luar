-- jit/141_multi_loop_retry.lua -- a rejected hot loop must not disable a
-- compileable sibling loop in the same Proto.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/141_multi_loop_retry: skipped (no native target)")
  return
end

jit.flush()
jit.on()

local str = "abcdefghij"          -- #str is not a J2-whitelisted operation
local function f (n)
  local s = 0
  for _ = 1, n do s = s + #str end
  local x = 0
  for j = 1, n do x = x + j end
  return s, x
end

local s, x = f(20000)
H.eq(s, 10 * 20000, "string-length loop result")
H.eq(x, 20000 * 20001 // 2, "sibling numeric loop result")
H.check(jit.stats().compiled >= 1,
        "sibling loop was not compiled after the first loop was rejected")
H.check(jit.stats().traces >= 1, "loop snapshots were not recorded")

print("jit/141_multi_loop_retry ok")
