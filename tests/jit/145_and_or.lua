-- jit/145_and_or.lua -- J2 compiles numeric TEST/TESTSET branches.
--
-- Lua compiles 'a and b' / 'a or b' to TEST/TESTSET followed by a JMP.
-- A number or table is always truthy, so the test has no value-level
-- effect, but the register still needs an entry guard: calling the same
-- compiled loop later with false/nil must fall back to the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/145_and_or: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("hotloop=1", "ir=0")
jit.on()

local function andloop (n)
  local s = 0
  for i = 1, n do
    if (i & 1) ~= 0 and i < n then s = s + i end
  end
  return s
end

local function orloop (n)
  local s = 0
  for i = 1, n do
    if (i & 1) == 0 or i < 10 then s = s + 1 end
  end
  return s
end

local function testsetloop (n)
  local s = 0
  for i = 1, n do
    local x = i and i or 0
    s = s + x
  end
  return s
end

local function truthy (v, n)
  local s = 0
  for i = 1, n do
    if v then s = s + 1 end
  end
  return s
end

local function testset_value (v, n)
  local s = 0
  for i = 1, n do
    s = s + (v and v or 0)
  end
  return s
end

local n = 2000
local want_and = 0
for i = 1, n do if (i & 1) ~= 0 and i < n then want_and = want_and + i end end
local want_or = 0
for i = 1, n do if (i & 1) == 0 or i < 10 then want_or = want_or + 1 end end

H.eq(andloop(n), want_and)
H.check(jit.stats().compiled >= 1, "and-loop did not compile")
H.eq(orloop(n), want_or)
H.eq(testsetloop(n), n * (n + 1) // 2)
H.check(jit.stats().compiled >= 3, "TEST/TESTSET loops did not compile")

-- The same Protos with a false/nil condition must leave native code.
H.eq(truthy(1, n), n)
H.eq(truthy(false, n), 0)
H.eq(truthy(nil, n), 0)
H.eq(testset_value(3, n), 3 * n)
H.eq(testset_value(false, n), 0)
H.eq(testset_value(nil, n), 0)

-- jit.off() must still interpret the TEST paths correctly.
jit.off()
H.eq(andloop(n), want_and)
H.eq(orloop(n), want_or)
H.eq(testsetloop(n), n * (n + 1) // 2)
H.eq(truthy(n, n), n)
H.eq(testset_value(5, n), 5 * n)
jit.on()

jit.opt.start("hotloop=56", "ir=0")
jit.flush()
print("jit/145_and_or ok")
