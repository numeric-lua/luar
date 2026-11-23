-- jit/182_jit_off_hot.lua -- jit.off() must not consume a loop's hot state.
--
-- The backedge hook kept counting (and recorded the backedge in luar_seen)
-- while tracing was disabled.  A loop that reached its threshold during
-- jit.off() was then permanently seen, so a later jit.on() left it on the
-- interpreter until jit.flush() or a new Proto.  Disabled tracing now
-- returns before touching the counters, and jit.on() resumes compilation.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/182_jit_off_hot: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=0", "tint=0")

local function sum (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end

jit.off()
H.eq(sum(100), 5050, "interpreter result")
H.eq(jit.stats().compiled, 0, "jit.off() compiled a loop")

jit.on()
H.eq(sum(100), 5050, "first call after jit.on()")
H.check(jit.stats().compiled >= 1,
        "a loop that became hot while jit was off did not compile after jit.on()")
H.check(jit.stats().sig_hits > 0, "the resumed loop never entered native code")
H.eq(sum(100), 5050, "second call after jit.on()")

-- Turning tracing off again must not run native code, and turning it back
-- on must keep using the already-installed loop.
jit.off()
H.eq(sum(100), 5050, "jit.off() result with an installed loop")
jit.on()
H.eq(sum(100), 5050, "jit.on() resumes the installed loop")
H.check(jit.stats().sig_hits > 1, "the installed loop did not resume")

-- A partially hot loop also reaches the threshold after resuming.
jit.flush()
jit.opt.start("hotloop=4", "ir=0")
local function count (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end
jit.off()
H.eq(count(2), 3, "partial interpreter run")
jit.on()
H.eq(count(100), 5050, "partial loop after resume")
H.check(jit.stats().compiled >= 1, "partially hot loop did not compile")

jit.flush()
jit.opt.start("hotloop=56", "ir=0")
jit.on()
print("jit/182_jit_off_hot ok")
