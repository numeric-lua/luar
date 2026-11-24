-- jit/186_j2_table_sub.lua -- J2 single-source subtraction over a raw
-- table array: s = s - t[i], with integer and float accumulators.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/186_j2_table_sub: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end
local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

local n = 200
local ti, tf = {}, {}
for i = 1, n do
  ti[i] = i * 3 - 100
  tf[i] = i * 0.5 + 0.25
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local f1 = assert(load([[
  return function (t, n)
    local s = 0
    for i = 1, n do s = s - t[i] end
    return s
  end
]], "=tsub1"))()
local before = jit.stats().tint_hits
local got = f1(ti, n)
local used = jit.stats().tint_hits - before
local want = 0
for i = 1, n do want = want - ti[i] end
H.check(same(got, want), "integer table subtraction")
H.check(used > 0, "table subtraction intrinsic did not run")

local f2 = assert(load([[
  return function (t, n)
    local s = 0.0
    for i = 1, n do s = s - t[i] end
    return s
  end
]], "=tsub2"))()
before = jit.stats().tint_hits
got = f2(tf, n)
used = jit.stats().tint_hits - before
want = 0.0
for i = 1, n do want = want - tf[i] end
H.check(same(got, want), "float table subtraction")
H.check(used > 0, "float table subtraction intrinsic did not run")

local f3 = assert(load([[
  return function (t, n)
    local s = 0
    for i = n, 1, -1 do s = s - t[i] end
    return s
  end
]], "=tsub3"))()
before = jit.stats().tint_hits
got = f3(ti, n)
used = jit.stats().tint_hits - before
want = 0
for i = n, 1, -1 do want = want - ti[i] end
H.check(same(got, want), "descending table subtraction")
H.check(used > 0, "descending table subtraction intrinsic did not run")

jit.opt.start("ir=0", "hotloop=56", "hotexit=10", "tint=1")
jit.flush()
print("jit/186_j2_table_sub ok")
