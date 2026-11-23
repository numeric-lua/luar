-- compat/table_clear_large.lua -- table.clear() clears a large array part
-- in place and releases both array and hash references.
--
-- The array part is two contiguous runs (values and tags).  Clearing it with
-- memsets must preserve table.clear()'s observable contract: the table is
-- empty, collectable entries become unreachable, capacity is retained for
-- reuse, and no metamethod runs.
local H = dofile("tests/helpers.lua")

local clear = table.clear
H.check(type(clear) == "function", "table.clear is missing")
H.check(require("table.clear") == clear, "table.clear module disagrees")

local n = 200000
local t = table.new(n, 1000)
H.check(type(t) == "table", "table.new failed")

local weak = setmetatable({}, {__mode = "v"})
local weakn = 0
for i = 1, n do
  local v = {i}
  t[i] = v
  if i == 1 or i == n then weakn = weakn + 1; weak[weakn] = v end
end
for i = 1, 1000 do
  local v = {i}
  t["k" .. i] = v
  if i == 1 then weakn = weakn + 1; weak[weakn] = v end
end

table.clear(t)
H.eq(#t, 0, "array length after clear")
H.eq(next(t), nil, "table is not empty after clear")
H.eq(t[1], nil, "array entry survived clear")
H.eq(t[n], nil, "last array entry survived clear")
H.eq(t.k1, nil, "hash entry survived clear")

collectgarbage("collect")
for i = 1, weakn do
  H.eq(weak[i], nil, "cleared value was not released: " .. i)
end

-- Capacity is retained and the cleared table can be refilled normally.
for i = 1, n do t[i] = i end
H.eq(#t, n, "refilled array length")
H.eq(t[1], 1, "refilled first entry")
H.eq(t[n], n, "refilled last entry")

-- A metatable's __len is still honoured after clearing.
local meta = setmetatable({1, 2, 3}, {__len = function () return 99 end})
table.clear(meta)
H.eq(#meta, 99, "__len was bypassed or lost")
H.eq(next(meta), nil, "metatable table was not cleared")
H.eq(meta[1], nil, "metatable table entry survived")

print("compat/table_clear_large ok")
