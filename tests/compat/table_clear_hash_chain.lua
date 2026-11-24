-- compat/table_clear_hash_chain.lua -- table.clear must reset collision
-- links as well as keys and values.
--
-- A cleared node stays free, but its old 'next' offset used to survive.
-- The next insertion at that position inherited the stale chain, so a
-- later collision could walk an unrelated/cyclic chain: lookups lost keys
-- or the program hung.  Exercise clear/refill with mixed key kinds.
local H = dofile("tests/helpers.lua")

math.randomseed(12345)
local function refill (t, n)
  for i = 1, n do
    local k = (i % 2 == 0) and ("n" .. i) or (i * 13)
    t[k] = i
  end
  for i = 1, n do
    local k = (i % 2 == 0) and ("n" .. i) or (i * 13)
    H.eq(t[k], i, "refill lookup")
  end
end

for trial = 1, 100 do
  local t = table.new(0, 64)
  local n = math.random(1, 200)
  local keys = {}
  for i = 1, n do
    local k
    if i % 3 == 0 then k = "k" .. i
    elseif i % 3 == 1 then k = i * 7
    else k = {} end
    keys[i] = k
    t[k] = i
  end
  table.clear(t)
  for k, v in pairs(t) do
    error("table.clear left a key: " .. tostring(k) .. "=" .. tostring(v))
  end
  H.eq(#t, 0, "# after clear")
  refill(t, n)
  table.clear(t)
  H.eq(#t, 0, "# after second clear")
end

-- A few repeated clears on the same table must also stay consistent.
local u = table.new(0, 32)
for round = 1, 50 do
  for i = 1, 40 do u[("r" .. round .. ":" .. i)] = i end
  table.clear(u)
  H.eq(#u, 0, "repeated clear")
end

print("compat/table_clear_hash_chain ok")
