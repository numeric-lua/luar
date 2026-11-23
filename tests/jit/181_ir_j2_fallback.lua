-- jit/181_ir_j2_fallback.lua -- ir=1 enables the J4 for-loop tracer, but it
-- must not disable the J2 while/repeat emitter for shapes J4 cannot record.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/181_ir_j2_fallback: skipped (no native target)")
  return
end

local function while_sum (n)
  local i, s = 0, 0
  while i < n do s = s + i; i = i + 1 end
  return s
end

local function repeat_sum (n)
  local i, s = 0, 0
  repeat s = s + i; i = i + 1 until i >= n
  return s
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()

H.eq(while_sum(1000), 499500)
H.check(jit.stats().compiled >= 1,
        "J2 did not compile a while loop under ir=1")

jit.flush()
H.eq(repeat_sum(1000), 499500)
H.check(jit.stats().compiled >= 1,
        "J2 did not compile a repeat loop under ir=1")

-- A float-controlled for loop is another J4-rejected shape; it must reach
-- J2 rather than staying interpreted.
jit.flush()
local function float_for (n)
  local s = 0
  for i = 1.0, n do s = s + i end
  return s
end
H.eq(float_for(1000), 500500.0)
H.check(jit.stats().compiled >= 1,
        "J2 did not compile a float for loop under ir=1")

jit.flush()
jit.opt.start("ir=0", "hotloop=56", "hotexit=10")
jit.on()
print("jit/181_ir_j2_fallback ok")
