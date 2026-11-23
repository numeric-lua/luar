-- jit/161_three_table_sum.lua -- three raw array reads in one numeric loop.
--
-- The generic J2 table decoder allows two raw reads, so the common
-- dot-product shape `s = s + a[i] + b[i] + c[i]` stayed interpreted.  A
-- dedicated C intrinsic now runs the remaining iterations after re-checking
-- each table's array shape and every element tag.  Short tables, metatables
-- and non-numeric elements must fall back without changing the result.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/161_three_table_sum: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function mk (n, base, step)
  local t = {}
  for i = 1, n do t[i] = base + (i - 1) * step end
  return t
end

jit.opt.start("ir=0", "hotloop=1", "table=1", "tint=1")

-- Integer arrays and an integer accumulator.
local function sum3 (n)
  local a, b, c = mk(n, 1, 0), mk(n, 4, 1), mk(n, 7, 1)
  local s = 0
  for i = 1, n do s = s + a[i] + b[i] + c[i] end
  return s
end

jit.flush()
jit.off()
local want = sum3(200)
jit.flush()
jit.on()
local before = jit.stats().tint_hits
local got = sum3(200)
local st = jit.stats()
H.eq(got, want, "integer three-table sum")
H.check(st.tint_hits > before, "three-table intrinsic did not run")
H.check(st.compiled >= 1, "three-table loop did not compile")

-- Mixed element flavours promote the accumulator exactly once.
local function fsum3 (n)
  local a, b, c = mk(n, 1.5, 0.5), mk(n, 4, 1), mk(n, 7, 1)
  local s = 0
  for i = 1, n do s = s + a[i] + b[i] + c[i] end
  return s
end
jit.flush()
jit.off()
local fwant = fsum3(200)
jit.flush()
jit.on()
local fgot = fsum3(200)
H.eq(math.type(fgot), "float", "mixed sum did not promote")
H.eq(bits(fgot), bits(fwant), "mixed three-table sum")

-- The same table may be read three times.
local function triple (n)
  local a = mk(n, 3, 1)
  local s = 0
  for i = 1, n do s = s + a[i] + a[i] + a[i] end
  return s
end
jit.flush()
jit.off()
local twant = triple(200)
jit.flush()
jit.on()
H.eq(triple(200), twant, "three reads from the same table")

-- A metatable disables the raw intrinsic; the interpreter must still be
-- exact and the loop must not keep retrying the failing intrinsic.
local function meta_sum (n)
  local a, b, c = mk(n, 1, 0), mk(n, 4, 1), mk(n, 7, 1)
  setmetatable(a, { __index = function () return 0 end })
  local s = 0
  for i = 1, n do s = s + a[i] + b[i] + c[i] end
  return s
end
jit.flush()
jit.off()
local mwant = meta_sum(200)
jit.flush()
jit.on()
local e0 = jit.stats().exits
H.eq(meta_sum(200), mwant, "metatable fallback result")
H.check(jit.stats().exits >= e0, "metatable fallback lost its exit count")

-- A non-numeric element late in the array raises the interpreter's exact
-- error; the intrinsic must not write a partial accumulator.
local function bad_sum ()
  local a, b, c = mk(100, 1, 0), mk(100, 4, 1), mk(100, 7, 1)
  a[80] = "not a number"
  local s = 0
  for i = 1, 100 do s = s + a[i] + b[i] + c[i] end
  return s
end
jit.flush()
jit.off()
local wok, werr = pcall(bad_sum)
jit.flush()
jit.on()
local gok, gerr = pcall(bad_sum)
H.eq(gok, wok, "bad element error state")
H.eq(tostring(gerr), tostring(werr), "bad element error object")

jit.opt.start("table=0", "tint=0")
jit.flush()
print("jit/161_three_table_sum ok")
