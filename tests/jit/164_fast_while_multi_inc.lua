-- jit/164_fast_while_multi_inc.lua -- the fast while peephole keeps exactly
-- one step for the induction variable.
--
-- The scanner accepted every ADDI that wrote the loop index and kept only
-- the last immediate as d.fw_step.  `while i < n do s = s + i; i = i + 1;
-- i = i + 1 end` therefore ran with step 1 instead of 2 and produced a
-- different accumulator (and index) from the interpreter.  A second
-- induction write now makes the fast shape fall back to the generic
-- emitter/interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/164_fast_while_multi_inc: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function same (a, b)
  if type(a) ~= type(b) then return false end
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=0")

local function run (f, n)
  jit.flush()
  jit.off()
  local ws, wi = f(n)
  jit.flush()
  jit.on()
  local gs, gi = f(n)
  return ws, wi, gs, gi, jit.stats()
end

local cases = {
  {
    "two equal increments",
    function (n)
      local i, s = 0, 0
      while i < n do
        s = s + i
        i = i + 1
        i = i + 1
      end
      return s, i
    end,
  },
  {
    "two different increments",
    function (n)
      local i, s = 0, 0
      while i < n do
        s = s + i * i
        i = i + 2
        i = i + 3
      end
      return s, i
    end,
  },
  {
    "two increments, float accumulator",
    function (n)
      local i, s = 0, 0.5
      while i < n do
        s = s + i
        i = i + 1
        i = i + 2
      end
      return s, i
    end,
  },
}

for _, case in ipairs(cases) do
  local name, f = case[1], case[2]
  local ws, wi, gs, gi = run(f, 220)
  H.check(same(ws, gs) and same(wi, gi),
          name .. ": native result differs from the interpreter")
  -- A generic branch emitter may still compile this shape correctly; the
  -- regression is only that the single-step fast peephole must not claim
  -- it.  The interpreter comparison above is the observable contract.
end

-- A single induction write is still the supported fast shape.
local function single (n)
  local i, s = 0, 0
  while i < n do
    s = s + i
    i = i + 1
  end
  return s, i
end

local ws, wi, gs, gi, st = run(single, 220)
H.eq(ws, gs, "single increment sum")
H.eq(wi, gi, "single increment index")
H.check(st.compiled >= 1, "single increment loop did not compile")
H.check(st.sig_hits > 0, "single increment loop did not run natively")

jit.flush()
jit.opt.start("hotloop=56", "ir=0", "table=0", "tint=0")
print("jit/164_fast_while_multi_inc ok")
