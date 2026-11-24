-- jit/203_table_unary_store.lua -- native `dst[i] = -src[i]` and
-- `dst[i] = ~src[i]` over raw table arrays.
--
-- Unary minus keeps integer wrap or flips the float sign; bitwise not is
-- integer-only.  Float bitwise conversions, non-numeric sources and
-- destination metatables stay on the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/203_table_unary_store: skipped (no native target)")
  return
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    return string.pack("d", a) == string.pack("d", b)
  end
  return a == b
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 200
local I, F = {}, {}
for i = 1, N do
  I[i] = i - 100
  F[i] = i - 100 + 0.5
end

local function run (name, f, src)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f({}, src, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f({}, src, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 1, N do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

local function neg (dst, src, n)
  for i = 1, n do dst[i] = -src[i] end
  return dst
end

local function bnot (dst, src, n)
  for i = 1, n do dst[i] = ~src[i] end
  return dst
end

run("unary minus integer", neg, I)
run("unary minus float", neg, F)
run("bitwise not", bnot, I)

-- In-place unary minus.
local p1, p2 = {}, {}
for i = 1, N do p1[i] = I[i]; p2[i] = I[i] end
local function inplace (a, n)
  for i = 1, n do a[i] = -a[i] end
  return a
end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(p1, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
inplace(p2, N)
H.check(jit.stats().tint_hits > before, "in-place unary minus did not run")
for i = 1, N do H.check(same(p2[i], p1[i]), "in-place element " .. i) end

-- Float bitwise not falls back and raises.
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local ok = pcall(function ()
  local d = {}
  for i = 1, N do d[i] = ~F[i] end
  return d
end)
H.check(not ok, "float bitwise not must raise")

-- Destination metatable keeps __newindex in the interpreter.
local seen = 0
local meta = setmetatable({}, {
  __newindex = function (t, k, v) seen = seen + 1; rawset(t, k, v) end,
})
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
for i = 1, N do meta[i] = -I[i] end
H.eq(jit.stats().tint_hits, before, "metatable destination must not use the intrinsic")
H.eq(seen, N, "__newindex call count")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/203_table_unary_store ok")
