-- jit/130_fast_alias.lua -- fast while/repeat paths must not cache a limit
-- that the loop body also updates.
--
-- The fast paths keep the limit in a register for the whole run.  A source
-- loop may reuse one local as both the limit and the accumulator (or as the
-- induction variable); the native loop then compares against the stale
-- limit instead of the value the body writes.  Those shapes must fall back
-- to the generic emitter, which reloads the slots every iteration.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/130_fast_alias: skipped (no native target)")
  return
end

local function run (name, src, n)
  local ref = assert(load(src, "=aliasref"))()
  jit.flush()
  jit.off()
  local want = table.pack(ref(n))
  local f = assert(load(src, "=aliasjit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local got = table.pack(f(n))
  local again = table.pack(f(n))
  for i = 1, want.n do
    H.eq(got[i], want[i], name .. ": first call result " .. i)
    H.eq(again[i], want[i], name .. ": native call result " .. i)
  end
end

-- while: s is both the limit and the accumulator
local while_alias = [[
  return function (n)
    local i, s = 1, n
    while i <= s do
      s = s - i
      i = i + 1
    end
    return s, i
  end
]]
run("while limit == accumulator", while_alias, 10000)

-- repeat: same alias, with the accumulator updated after the increment so
-- the fast repeat pattern matches.
local repeat_alias = [[
  return function (n)
    local i, s = 1, n
    repeat
      i = i + 1
      s = s - i
    until i > s
    return s, i
  end
]]
run("repeat limit == accumulator", repeat_alias, 10000)

-- while where the limit is also the induction variable
local idx_alias = [[
  return function (n)
    local s, i = n, 1
    while i <= i do
      s = s - 1
      i = i + 1
      if i > 60000 then break end
    end
    return s, i
  end
]]
run("while limit == index", idx_alias, 1)

-- A distinct limit must keep using the fast path.
local distinct = [[
  return function (n)
    local s, i = 0, 1
    while i <= n do
      s = s - i
      i = i + 1
    end
    return s, i
  end
]]
local f = assert(load(distinct, "=aliasok"))()
jit.flush()
jit.opt.start("ir=0")
jit.on()
H.eq(f(10000), -50005000)
H.eq(f(10000), -50005000)
H.check(jit.stats().compiled >= 1, "distinct-limit while loop did not compile")

jit.flush()
print("jit/130_fast_alias ok")
