-- jit/129_type_stable.lua -- a branch must not change a register's type
-- between native iterations.
--
-- J2 guards tags at loop entry and then keeps values in their stack slots
-- without re-checking them on every iteration.  A register that is a float
-- when the loop becomes hot and later becomes an integer (or vice versa)
-- would make the next iteration read the payload with the wrong type.  Such
-- loops must fall back to the interpreter; stable branch loops still run
-- natively.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/129_type_stable: skipped (no native target)")
  return
end

local function run (name, src, n)
  local ref = assert(load(src, "=typeref"))()
  jit.flush()
  jit.off()
  local want = table.pack(ref(n))
  local f = assert(load(src, "=typejit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local got = table.pack(f(n))
  local again = table.pack(f(n))
  for i = 1, want.n do
    H.eq(got[i], want[i], name .. ": first call result " .. i)
    H.eq(again[i], want[i], name .. ": native call result " .. i)
    H.eq(math.type(got[i]), math.type(want[i]),
         name .. ": first call type " .. i)
    H.eq(math.type(again[i]), math.type(want[i]),
         name .. ": native call type " .. i)
  end
  return f
end

-- Float accumulator that becomes an integer late in the loop.  The branch
-- body that is live when the loop becomes hot is the float one, so the
-- entry guard matches and a naive emitter commits to a float register.
local for_src = [[
  return function (n)
    local x = 1.5
    local s = 0.0
    for i = 1, n do
      if i > 500 then x = 7 else x = 1.5 end
      s = s + x
    end
    return s, x
  end
]]
run("for float-to-int", for_src, 1000)

-- Same shape with a while loop.
local while_src = [[
  return function (n)
    local x = 1.5
    local s, i = 0.0, 1
    while i <= n do
      if i > 500 then x = 7 else x = 1.5 end
      s = s + x
      i = i + 1
    end
    return s, x, i
  end
]]
run("while float-to-int", while_src, 1000)

-- An integer accumulator that becomes a float after a threshold.  The
-- branch layout makes the integer body the hot one, so this also exercises
-- the guard signature after the type changes.
local int_src = [[
  return function (n)
    local x = 3
    local s = 0
    for i = 1, n do
      if i > 500 then x = 1.25 else x = 3 end
      s = s + x
    end
    return s, x
  end
]]
run("int-to-float", int_src, 1000)

-- A branch that only ever uses one type per register must still compile:
-- over-rejection would turn the fix into a performance regression.
local stable_src = [[
  return function (n)
    local s = 0
    for i = 1, n do
      if i > 500 then s = s + 2 else s = s + 1 end
    end
    return s
  end
]]
local stable = run("stable branch", stable_src, 1000)
H.check(jit.stats().compiled >= 1,
        "a type-stable branch loop was not compiled")

-- Mixed int/float arithmetic is stable as long as every register keeps one
-- type; this used to be (and must remain) a compiled shape.
local mixed_src = [[
  return function (n)
    local s = 0.0
    for i = 1, n do
      if i < 300 then s = s + 1.0 else s = s + 2.0 end
    end
    return s
  end
]]
local mixed = run("stable mixed", mixed_src, 1000)
H.check(jit.stats().compiled >= 1,
        "a type-stable mixed branch loop was not compiled")

jit.flush()
jit.opt.start("ir=0")
print("jit/129_type_stable ok")
