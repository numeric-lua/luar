-- compat/table_clear.lua -- table.clear drops all keys without metamethods
-- and keeps the table usable.
local H = dofile("tests/helpers.lua")

local t = table.new(16, 16)
for i = 1, 8 do t[i] = i end
for i = 1, 8 do t["k" .. i] = i end
t[1.5] = "float key"
H.eq(#t, 8)
H.eq(table.clear(t), nil)
H.eq(next(t), nil)
H.eq(#t, 0)
H.eq(rawlen(t), 0)

-- It must stay usable and preserve whatever capacity it had.
for i = 1, 16 do t[i] = i * 2 end
for i = 1, 8 do t["n" .. i] = i end
H.eq(t[16], 32)
H.eq(t.n8, 8)
table.clear(t)
H.eq(next(t), nil)

-- No __index/__newindex call may be made while clearing.
local log = {}
local mt = {
  __index = function () log[#log + 1] = "index"; return 0 end,
  __newindex = function () log[#log + 1] = "newindex" end,
}
local u = setmetatable({ a = 1, b = 2, [1] = 3 }, mt)
table.clear(u)
H.eq(next(u), nil)
H.eq(#log, 0, "table.clear invoked a metamethod")

-- Weak tables and repeated clears are fine.
local w = setmetatable({}, { __mode = "v" })
w[1], w.x = {}, {}
table.clear(w)
H.eq(next(w), nil)
table.clear(w)
table.clear(w)
H.eq(next(w), nil)

-- A table used as a key must be released, not kept alive by a dead node.
local key = {}
local kt = table.new(0, 1)
kt[key] = 1
table.clear(kt)
H.eq(next(kt), nil)

H.raises(function () table.clear(42) end, "table expected")
print("compat/table_clear ok")
