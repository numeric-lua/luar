-- jit/179_j4_float_for.lua -- J4 must not use its integer FORLOOP emitter
-- for a float-controlled for loop.
--
-- The recorder used to accept a float-controlled loop whose body contained
-- only integer operations.  J4 then treated the double payloads in R[A..A+2]
-- as an integer count/step/index: the loop never terminated.  The recorder
-- now rejects the shape, and ir=1 falls back to J2/interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/179_j4_float_for: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()

local function int_body (n)
  local s = 0
  for i = 1.0, n do s = s + 1 end
  return s
end
H.eq(int_body(2000), 2000)
H.check(jit.stats().ir_compiled == 0,
        "float-controlled FORLOOP was installed as an integer IR trace")

local function float_body (n)
  local s = 0.0
  for i = 0.5, n, 0.5 do s = s + i end
  return s
end
H.eq(float_body(100), 10050.0)

local function descending (n)
  local s = 0
  for i = n, 1.0, -1 do s = s + i end
  return s
end
H.eq(descending(200), 20100)

-- An integer-controlled loop must still be allowed to use J4.
local function integer_loop (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end
H.eq(integer_loop(1000), 500500)
H.check(jit.stats().ir_compiled >= 1,
        "integer for loop was not compiled by J4")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/179_j4_float_for ok")
