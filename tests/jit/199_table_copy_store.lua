-- jit/199_table_copy_store.lua -- native `dst[i] = src[i]` element copies
-- over raw table arrays.
--
-- The scaled-store helper also handles the plain copy shape, including
-- in-place copies, destination growth and upvalue sources.  Metatables and
-- non-numeric values stay on the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/199_table_copy_store: skipped (no native target)")
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
local S = {}
for i = 1, N do S[i] = i * 1.25 - 3 end
local US = S

local function run (name, f, src, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f({}, src, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f({}, src, ...)
  local used = jit.stats().tint_hits - before
  H.check(used > 0, name .. ": intrinsic did not run")
  for i = 1, N do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
  return got
end

local function copy (dst, src, n)
  for i = 1, n do dst[i] = src[i] end
  return dst
end

local function copy_up (dst, src, n)
  for i = 1, n do dst[i] = US[i] end
  return dst
end

local function copy_down (dst, src, n)
  for i = n, 1, -1 do dst[i] = src[i] end
  return dst
end

local function copy_step (dst, src, n)
  for i = 1, n, 3 do dst[i] = src[i] end
  return dst
end

run("copy", copy, S, N)
run("upvalue source", copy_up, S, N)
run("descending copy", copy_down, S, N)
run("strided copy", copy_step, S, N)

-- In-place copy.
local a1, a2 = {}, {}
for i = 1, N do a1[i] = S[i]; a2[i] = S[i] end
local function inplace (a, n)
  for i = 1, n do a[i] = a[i] end
  return a
end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(a1, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
inplace(a2, N)
H.check(jit.stats().tint_hits > before, "in-place copy did not run")
for i = 1, N do H.check(same(a2[i], a1[i]), "in-place element " .. i) end

-- Destination growth preserves hash keys.
local function grow (dst, src, n)
  for i = 1, n do dst[i] = src[i] end
  return dst
end
local g1 = { marker = "keep", [N + 4] = 7 }
local g2 = { marker = "keep", [N + 4] = 7 }
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
grow(g1, S, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
grow(g2, S, N)
H.eq(g2.marker, "keep", "hash key preserved")
H.eq(g2[N + 4], 7, "later array key preserved")
for i = 1, N do H.check(same(g2[i], g1[i]), "grown element " .. i) end

-- Destination metatable keeps __newindex in the interpreter.
local seen = 0
local meta = setmetatable({}, {
  __newindex = function (t, k, v) seen = seen + 1; rawset(t, k, v) end,
})
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
copy(meta, S, N)
H.eq(jit.stats().tint_hits, before, "metatable destination must not use the intrinsic")
H.eq(seen, N, "__newindex call count")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/199_table_copy_store ok")
