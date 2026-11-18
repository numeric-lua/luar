-- jit/24_table_hash_intrinsic.lua -- keyed raw table fill/sum intrinsics.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/24_table_hash_intrinsic: skipped (no native target)")
  return
end

local keys = {}
for i = 1, 5000 do keys[i] = "key:" .. i end

local function fill (t, ks, n)
  for i = 1, n do t[ks[i]] = i end
end
local function sum (t, ks, n)
  local s = 0
  for i = 1, n do s = s + t[ks[i]] end
  return s
end

local t = {}
fill(t, keys, 5000)
H.eq(t["key:1"], 1)
H.eq(t["key:5000"], 5000)
H.eq(sum(t, keys, 5000), 12502500)

-- upvalue key table shape
local ukeys = keys
local function ufill (t)
  for i = 1, 5000 do t[ukeys[i]] = i end
end
local function usum (t)
  local s = 0
  for i = 1, 5000 do s = s + t[ukeys[i]] end
  return s
end
local u = {}
ufill(u)
H.eq(u["key:5000"], 5000)
H.eq(usum(u), 12502500)
H.check(jit.stats().compiled >= 2, "keyed table intrinsics did not compile")

local f = {}
for i = 1, 1000 do f["f" .. i] = i + 0.5 end
local fkeys = {}
for i = 1, 1000 do fkeys[i] = "f" .. i end
local function fsum (t, ks, n)
  local s = 0.0
  for i = 1, n do s = s + t[ks[i]] end
  return s
end
H.eq(fsum(f, fkeys, 1000), 501000.0)

jit.off()
H.eq(sum(t, keys, 5000), 12502500)
jit.on()
print("jit/24_table_hash_intrinsic ok")
