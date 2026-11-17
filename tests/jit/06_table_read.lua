-- jit/06_table_read.lua -- read-only raw array accesses in native loops.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
jit.opt.start("table=1")
if jit.status() == false then
  print("jit/06_table_read: skipped (no native target)")
  return
end

local function fill (t, n, f)
  for i = 1, n do t[i] = f(i) end
end

local t = {}
fill(t, 20000, function (i) return i end)
local function sum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s
end
local function sum_shift (t, n)
  local s = 0
  for i = 1, n do s = s + t[i + 1] end
  return s
end

H.eq(sum(t, 20000), 200010000)
H.check(jit.stats().compiled >= 1, "array sum did not compile")

-- a float accumulator uses the same fused lookup with a conversion
local function sumf (t, n)
  local s = 0.0
  for i = 1, n do s = s + t[i] end
  return s
end
H.eq(sumf(t, 20000), 200010000.0)

-- a non-fused use of the loaded value still works
local function sum2 (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end
H.eq(sum2(t, 20000), 400020000)

-- non-integer elements must fall back without changing the result
local ft = {}
fill(ft, 5000, function (i) return i * 0.5 end)
H.eq(sum(ft, 5000), 6251250.0)

local ht = {}
fill(ht, 5000, function (i) return i end)
ht[100] = nil
H.raises(function () return sum(ht, 5000) end, "arithmetic on a nil")

local st = {}
fill(st, 5000, function (i) return i end)
st[100] = "x"
H.raises(function () return sum(st, 5000) end, "attempt to add")

-- a shifted key exercises the key register path
H.eq(sum_shift(t, 10000), 50015000)

-- an integer table read must not change the interpreter result after jit.off
jit.off()
H.eq(sum(t, 20000), 200010000)
jit.on()

jit.opt.start("table=0")
print("jit/06_table_read ok")
