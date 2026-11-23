-- jit/160_straight_type_change.lua -- a straight-line loop body that changes
-- a loop-carried register's type must not be compiled with one static type.
--
-- The old J2 decoder guarded tags only at the loop entry.  With hotloop=1 it
-- recorded the first backedge, when `u` was still an integer, then emitted
-- `u = s - i` as an integer subtraction.  On the next native iteration `u`
-- held a double; the code read the double payload as an integer and the
-- results diverged from the interpreter.  The type-stability check used to
-- run only for branch bodies, so this straight-line shape slipped through.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/160_straight_type_change: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function pack (...)
  return {n = select("#", ...), ...}
end

local function same (a, b)
  for i = 1, a.n do
    if math.type(a[i]) ~= math.type(b[i]) then return false end
    if math.type(a[i]) == "float" then
      if a[i] ~= a[i] and b[i] ~= b[i] then
        -- NaN payloads are not compared here; this test has no NaNs.
      elseif bits(a[i]) ~= bits(b[i]) then
        return false
      end
    elseif a[i] ~= b[i] then
      return false
    end
  end
  return true
end

-- hotloop=1 is part of the bug: compilation happens at the first backedge,
-- while the mixed-type body has already changed some slots.
jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=0")

local sources = {
  [[
    return function (n)
      local s, t, u = 100, 0.5, -1
      for i = 1, n do
        t = u + i * 1.5
        u = s - i
        s = s + ((t + i) * -1)
      end
      return s, t, u
    end
  ]],
  [[
    return function (n)
      local s, t, u = 100, 0.5, -1
      for i = 1, n do
        t = t - s
        u = u * 7
        t = u + i * 1.5
        u = s - i
        s = s + ((t + i) * -1)
      end
      return s, t, u
    end
  ]],
}

for case, src in ipairs(sources) do
  local chunk = assert(load(src, "=typechange" .. case))
  local f = chunk()
  jit.flush()
  jit.off()
  local want = pack(f(188))
  jit.flush()
  jit.on()
  local got = pack(f(188))
  H.eq(got.n, want.n, "case " .. case .. ": result count")
  H.check(same(want, got),
          "case " .. case .. ": loop-carried type change changed the result")
  H.check(jit.stats().exits == 0,
          "case " .. case .. ": the loop was entered natively")
end

-- A type-stable straight-line loop still compiles and runs natively.
local stable = assert(load([[
  return function (n)
    local s = 0
    for i = 1, n do s = s + i end
    return s
  end
]], "=typechange-stable"))()
jit.flush()
jit.on()
H.eq(stable(100), 5050)
H.check(jit.stats().compiled >= 1, "stable loop did not compile")
H.check(jit.stats().sig_hits > 0, "stable loop did not run natively")

jit.flush()
jit.opt.start("hotloop=56")
print("jit/160_straight_type_change ok")
