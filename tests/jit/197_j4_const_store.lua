-- jit/197_j4_const_store.lua -- J4 must not treat a constant RK(C) as a
-- stack slot in OP_SETTABLE.
--
-- `t[i] = nil` encodes nil as a constant-table index.  The general dynamic
-- store path used that index as a register, so a hot loop wrote the value
-- of some unrelated local and left the table full.  Constant boolean,
-- integer, float and string values have dedicated paths, but the generic
-- path must still reject a constant instead of reading a slot.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/197_j4_const_store: skipped (no native target)")
  return
end

local function count (t)
  local c = 0
  for _ in pairs(t) do c = c + 1 end
  return c
end

local function run (name, f, fill)
  local t = {}
  if fill then fill(t) end
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1", "table=0")
  jit.on()
  f(t, 1000)
  return t
end

local function clear (t, n)
  for i = 1, n do t[i] = nil end
end

local t = run("nil", clear, function (tab)
  for i = 1, 1000 do tab[i] = i end
end)
H.eq(count(t), 0, "nil store leaves an empty table")
H.eq(t[1000], nil, "nil store value")

-- Constant non-nil values must be correct too.
local function set_true (tab, n)
  for i = 1, n do tab[i] = true end
end
local b = run("bool", set_true)
H.eq(b[1], true, "constant boolean store")
H.eq(b[1000], true, "constant boolean store last")

local function set_int (tab, n)
  for i = 1, n do tab[i] = 7 end
end
local i7 = run("int", set_int)
H.eq(i7[1], 7, "constant integer store")
H.eq(i7[1000], 7, "constant integer store last")

local function set_str (tab, n)
  for i = 1, n do tab[i] = "x" end
end
local s = run("string", set_str)
H.eq(s[1], "x", "constant string store")
H.eq(s[1000], "x", "constant string store last")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "table=0")
print("jit/197_j4_const_store ok")
