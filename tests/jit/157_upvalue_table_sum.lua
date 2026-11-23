-- jit/157_upvalue_table_sum.lua -- raw array sums over a closure upvalue.
--
-- `local t = ...; local function f(n) ... s = s + t[i] ... end` is the
-- common shape for module-level buffers.  The table register came from
-- GETUPVAL, so the register-table intrinsic did not match and the loop ran
-- on the interpreter.  A dedicated intrinsic resolves the upvalue at each
-- entry, preserving reassignment and metatable fallback.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/157_upvalue_table_sum: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end

jit.flush()
jit.off()
jit.opt.start("hotloop=1", "table=0", "tint=1")

local tab = {}
for i = 1, 3000 do tab[i] = i end

local function sum (n)
  local s = 0
  for i = 1, n do s = s + tab[i] end
  return s
end

local function sum_keep (n)
  local s, x = 0, 0
  for i = 1, n do
    x = tab[i]
    s = s + x
  end
  return s, x
end

local mixed = {1, 2, 3.5, 4}
local function sum_mixed (n)
  local s = 0
  for i = 1, n do s = s + mixed[i] end
  return s
end

local function sum_float (n)
  local s = 0.0
  for i = 1, n do s = s + tab[i] end
  return s
end

local slot
local function sum_rebind (n)
  local s = 0
  for i = 1, n do s = s + slot[i] end
  return s
end

local meta = setmetatable({1, 2, 3}, {
  __index = function (_, k) return k * 10 end,
})
local function sum_meta (n)
  local s = 0
  for i = 1, n do s = s + meta[i] end
  return s
end

local holey = {1, 2, 3}
holey[4] = false
local function sum_bad (n)
  local s = 0
  for i = 1, n do s = s + holey[i] end
  return s
end

jit.flush()
jit.on()

local before = jit.stats().tint_hits
H.eq(sum(2000), 2000 * 2001 // 2, "upvalue table sum")
H.check(jit.stats().tint_hits > before, "upvalue sum did not use the intrinsic")
H.check(jit.stats().compiled >= 1, "upvalue sum loop did not compile")

local s, x = sum_keep(500)
H.eq(s, 500 * 501 // 2, "upvalue sum with a kept element")
H.eq(x, 500, "upvalue sum did not materialise the element slot")

local got = sum_mixed(4)
H.eq(got, 10.5, "mixed element sum")
H.eq(math.type(got), "float", "float element did not promote the accumulator")

local gf = sum_float(2000)
H.eq(gf, 2000 * 2001 // 2 + 0.0, "float accumulator sum")
H.eq(math.type(gf), "float", "float accumulator changed type")

-- The upvalue is resolved on every entry, not captured at compile time.
slot = {1, 2, 3}
H.eq(sum_rebind(3), 6, "first bound upvalue table")
slot = {10, 20, 30, 40}
H.eq(sum_rebind(4), 100, "rebound upvalue table")
slot = {7}
H.eq(sum_rebind(1), 7, "short rebound upvalue table")

-- A metatable or a non-number element must leave the result to T0.
local m = 0
for i = 1, 200 do m = m + (i <= 3 and i or i * 10) end
H.eq(sum_meta(200), m, "metatable fallback")
H.raises(function () return sum_bad(4) end, "arithmetic")
H.eq(sum(1000), 1000 * 1001 // 2, "sum after a fallback")

jit.opt.start("hotloop=56", "table=0", "tint=1")
jit.flush()
print("jit/157_upvalue_table_sum ok")
