-- jit/146_geti_result.lua -- fused GETI keeps its bytecode result register.
--
-- The interpreter writes R[A] = t[const] before folding it into the
-- accumulator.  J2 used to leave R[A] untouched, so a local assigned from
-- t[1] kept its pre-loop value when the loop ran natively.  The result is
-- observable after the loop and from later instructions in the body.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/146_geti_result: skipped (no native target)")
  return
end

local function f (t, n)
  local x = 1
  local s = 0
  for i = 1, n do
    x = t[1]
    s = s + x
  end
  return s, x
end

-- R[const] is read again after the fused add.
local function later (t, n)
  local x = 0
  local s = 0
  local y = 0
  for i = 1, n do
    x = t[2]
    s = s + x
    y = y + x
  end
  return s, y, x
end

-- The lookup result is also the accumulator; fusing it would double the
-- old value instead of computing t[1] + t[1].
local function same_reg (t, n)
  local x = 0
  for i = 1, n do
    x = t[1]
    x = x + x
  end
  return x
end

-- A float accumulator still leaves an integer element in R[A].
local function mixed (t, n)
  local x = 0
  local s = 0.0
  for i = 1, n do
    x = t[1]
    s = s + x
  end
  return s, x
end

jit.flush()
jit.opt.start("hotloop=1", "table=1", "tint=0", "ir=0")
jit.on()

local t = {7, 9}
H.eq(f(t, 200), 1400, 7)
H.eq(f(t, 200), 1400, 7)       -- cached native loop from the first iteration

H.eq(later(t, 200), 1800, 1800, 9)
H.eq(later(t, 200), 1800, 1800, 9)

H.eq(same_reg(t, 200), 14)
H.eq(same_reg(t, 200), 14)

local s, x = mixed(t, 200)
H.eq(s, 1400.0)
H.eq(math.type(s), "float")
H.eq(x, 7)
H.eq(math.type(x), "integer")

H.check(jit.stats().compiled >= 1, "GETI loop did not compile")
H.check(jit.stats().exits == 0, "GETI loop exited instead of running natively")

jit.opt.start("table=0")
jit.flush()
print("jit/146_geti_result ok")
