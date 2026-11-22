-- jit/136_generic_table_array.lua -- the opt-in generic table emitter must
-- use Lua 5.5's split array representation (an inverted Value array plus a
-- separate tag byte array), not 16-byte TValues.
--
-- Before this fix, the tag guard read the next value's low payload byte and
-- a store wrote a whole TValue over two array slots.  Shapes that the raw
-- fill/sum intrinsics do not cover (a scaled read, a scaled write) therefore
-- either exited on every iteration or corrupted neighbouring elements.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/136_generic_table_array: skipped (no native target)")
  return
end

jit.off()
jit.flush()

local function fill (t, n, f)
  for i = 1, n do t[i] = f(i) end
end

local t = {}
fill(t, 1000, function (i) return i end)

-- Force the generic emitter: table emission on, raw intrinsics off.
jit.opt.start("table=1", "tint=0")
jit.opt.start("hotloop=2")
jit.on()

local function sum2 (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end

H.eq(sum2(t, 200), 200 * 201, "scaled array read")
local e0 = jit.stats().exits
H.eq(sum2(t, 200), 200 * 201, "second scaled array read")
H.eq(jit.stats().exits, e0,
     "the generic table read exited instead of running natively")
H.check(jit.stats().compiled >= 1, "generic table read did not compile")

local function set2 (t, n)
  for i = 1, n do t[i] = i * 2 end
end

local u = {}
fill(u, 1000, function (i) return -i end)
set2(u, 200)
for i = 1, 200 do
  H.eq(u[i], i * 2, "integer array store at " .. i)
end
for i = 201, 1000 do
  H.eq(u[i], -i, "array store wrote outside its slot at " .. i)
end
H.eq(#u, 1000, "array store changed the table length")

local function setf (t, n)
  for i = 1, n do t[i] = i * 0.5 end
end

local v = {}
fill(v, 1000, function (i) return -i end)
setf(v, 200)
for i = 1, 200 do
  H.eq(v[i], i * 0.5, "float array store at " .. i)
end
for i = 201, 1000 do
  H.eq(v[i], -i, "float store wrote outside its slot at " .. i)
end

-- The same accessors are used by branch loops.
local function branch_sum (t, n)
  local s = 0
  for i = 1, n do
    if (i & 1) ~= 0 then s = s + t[i] else s = s - t[i] end
  end
  return s
end
H.eq(branch_sum(t, 200), -100, "branch array read")

jit.opt.start("table=0", "tint=1")
jit.opt.start("hotloop=56")
jit.flush()
print("jit/136_generic_table_array ok")
