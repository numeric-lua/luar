-- jit/187_affine_fill.lua -- native raw-table fills with an affine value.
--
-- `t[i] = i + k`, `t[i] = i * k` and `t[i] = k` are common initialization
-- loops.  J2 used to compile only `t[i] = i`; the C intrinsic now runs the
-- remaining ascending unit-step iterations with exact integer wrap, keeps
-- existing hash keys, and leaves metatables and unsupported scalars on the
-- interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/187_affine_fill: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 400

local function snapshot (t, n)
  local s = {}
  for i = 1, n do s[i] = t[i] end
  return s
end

local function run (name, src, n, setup)
  local f = assert(load(src, "=" .. name))()
  local a = setup and setup() or {}
  local b = setup and setup() or {}
  jit.flush()
  jit.off()
  local want = f(a, n)
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(b, n)
  local used = jit.stats().tint_hits - before
  local st = jit.stats()
  H.eq(got, b, name .. ": result table")
  local wa, wb = snapshot(a, n + 2), snapshot(b, n + 2)
  for i = 1, n + 2 do
    H.eq(wb[i], wa[i], name .. ": element " .. i)
  end
  H.check(used > 0, name .. ": fill intrinsic did not run")
  H.check(st.compiled > 0, name .. ": fill intrinsic did not compile")
  return b
end

-- Add a constant (ADDI), including the subtract spelling that the compiler
-- encodes as ADDI with a negative immediate.
run("affine_add", [[
  return function (t, n)
    for i = 1, n do t[i] = i + 3 end
    return t
  end
]], N)
run("affine_sub", [[
  return function (t, n)
    for i = 1, n do t[i] = i - 2 end
    return t
  end
]], N)

-- Multiply by a constant (MULK), with two's-complement wrap.
run("affine_mul", [[
  return function (t, n)
    for i = 1, n do t[i] = i * 1000003 end
    return t
  end
]], N)
run("affine_mul_zero", [[
  return function (t, n)
    for i = 1, n do t[i] = i * 0 end
    return t
  end
]], N)

-- Constant stores, positive and negative.
run("affine_const", [[
  return function (t, n)
    for i = 1, n do t[i] = 7 end
    return t
  end
]], N)
run("affine_const_neg", [[
  return function (t, n)
    for i = 1, n do t[i] = -2 end
    return t
  end
]], N)

-- Existing hash keys / higher array entries survive, exactly as the
-- interpreter's stores do.
local function setup ()
  local t = { marker = 11, [N + 3] = 99 }
  return t
end
local t = run("affine_hash", [[
  return function (t, n)
    for i = 1, n do t[i] = i + 1 end
    return t
  end
]], N, setup)
H.eq(t.marker, 11, "hash key preserved")
H.eq(t[N + 3], 99, "later array key preserved")

-- A non-unit step stays interpreted and remains correct.
local function step (t, n)
  local got = {}
  for i = 1, n, 2 do t[i] = i + 5 end
  return t
end
jit.off()
local a = {}
step(a, N)
jit.on()
local b = {}
jit.flush()
local before = jit.stats().tint_hits
step(b, N)
H.eq(jit.stats().tint_hits, before, "stepped fill must not use the intrinsic")
for i = 1, N do H.eq(b[i], a[i], "stepped fill element " .. i) end

-- A metatable with __newindex keeps control flow in the interpreter; the
-- intrinsic must not bypass the metamethod.
local seen = 0
local function metasetup ()
  return setmetatable({}, {
    __newindex = function (tt, k, v) seen = seen + 1; rawset(tt, k, v) end,
  })
end
local mf = assert(load([[
  return function (t, n)
    for i = 1, n do t[i] = i + 1 end
    return t
  end
]], "=affine_meta"))()
local mt = metasetup()
jit.flush()
jit.off()
mf(mt, 50)
local want_calls = seen
seen = 0
jit.flush()
jit.on()
local mt2 = metasetup()
before = jit.stats().tint_hits
mf(mt2, 50)
H.eq(jit.stats().tint_hits, before, "metatable fill must not use the intrinsic")
H.eq(seen, want_calls, "__newindex call count")
for i = 1, 50 do H.eq(mt2[i], i + 1, "metatable element " .. i) end

-- A scalar too large for the intrinsic's int field falls back cleanly.
local function wide (t, n, k)
  for i = 1, n do t[i] = i + k end
  return t
end
local big = math.maxinteger - 3
jit.off()
local wa = wide({}, N, big)
jit.on()
local wb = wide({}, N, big)
for i = 1, N do H.eq(wb[i], wa[i], "wide scalar element " .. i) end

-- Non-integer values (i + 1.5) are not affine integer fills.
local function frac (t, n)
  for i = 1, n do t[i] = i + 1.5 end
  return t
end
jit.off()
local fa = frac({}, N)
jit.on()
local fb = frac({}, N)
for i = 1, N do
  H.eq(fb[i], fa[i], "fractional fill element " .. i)
  H.eq(math.type(fb[i]), "float", "fractional fill type " .. i)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/187_affine_fill ok")
