-- jit/172_repeat_negative_step.lua -- bottom-tested repeat loops with a
-- negative step and body work before the increment.
--
-- The generic repeat emitter used a reusable local label for its exit.
-- A helper reused that label, so the fall-through path returned with eax
-- unset; the caller then re-ran the body one extra time (or looped).  The
-- exit now uses the same global epilogue label as ordinary while loops.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/172_repeat_negative_step: skipped (no native target)")
  return
end

local function run (name, src)
  local f = assert(load(src, "=repeatneg"))()
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
  jit.off()
  local want = { pcall(f) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
  jit.on()
  local got = { pcall(f) }
  H.eq(#got, #want, name .. ": result count")
  H.eq(got[1], want[1], name .. ": error status")
  for i = 2, #want do
    H.eq(got[i], want[i], name .. ": result " .. i)
  end
end

run("step -1 from 1", [[
return function ()
  local i, s = 1, 0
  repeat s = s - i; i = i - 1 until i < 0
  return s, i
end]])

run("step -1 from 3", [[
return function ()
  local i, s = 3, 0
  repeat s = s - i; i = i - 1 until i < 0
  return s, i
end]])

run("step -2 from 3", [[
return function ()
  local i, s = 3, 0
  repeat s = s - i; i = i - 2 until i < 0
  return s, i
end]])

run("step -1 until <=", [[
return function ()
  local i, s = 1, 0
  repeat s = s - i; i = i - 1 until i <= -1
  return s, i
end]])

run("step -1 constant body", [[
return function ()
  local i, s = 2, 0
  repeat s = s - 1; i = i - 1 until i < 0
  return s, i
end]])

-- Increment-first and positive-step repeats keep working.
run("increment first", [[
return function ()
  local i, s = 1, 0
  repeat i = i - 1; s = s - i until i < 0
  return s, i
end]])

run("positive step", [[
return function ()
  local i, s = 0, 0
  repeat s = s - i; i = i + 1 until i >= 20
  return s, i
end]])

-- A body that also writes the accumulator must still terminate correctly.
run("accumulator condition", [[
return function ()
  local i, s = 1, 0
  repeat s = s - i; i = i - 1 until s < 0
  return s, i
end]])

jit.flush()
print("jit/172_repeat_negative_step ok")
