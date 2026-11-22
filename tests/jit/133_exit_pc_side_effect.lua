-- jit/133_exit_pc_side_effect.lua -- a mid-loop guard exit must resume at
-- the comparison's bytecode PC, not at the function's first instruction.
--
-- The mixed integer/float comparison guard records the originating PC in
-- LuaR_Op.pc.  Decoding used to zero that field right after setting it, so a
-- guard exit restarted the whole function: the loop produced the right sum
-- but repeated every side effect before the loop.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/133_exit_pc_side_effect: skipped (no native target)")
  return
end

local calls = 0
local function run (n)
  calls = calls + 1
  local s = 0
  for i = 9007199254740991, 9007199254740991 + n do
    if i < 9007199254740992.0 then s = s + 1 end
  end
  return s
end

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.eq(run(100), 1, "first call result")
H.eq(calls, 1, "a guard exit must not restart the function")
H.check(jit.stats().compiled >= 1, "comparison loop did not compile")
H.eq(run(100), 1, "second call result")
H.eq(calls, 2, "one side effect per call")

jit.flush()
print("jit/133_exit_pc_side_effect ok")
