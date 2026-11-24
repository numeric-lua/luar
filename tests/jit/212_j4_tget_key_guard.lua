-- jit/212_j4_tget_key_guard.lua -- J4 dynamic GETTABLE always guards the
-- key as an integer at the read point.
--
-- A record-time check of the end-of-iteration stack value is not enough:
-- an earlier boolean/string producer can set seen[key], so a later TGET
-- would pass the raw payload as an integer index.  The key guard is now
-- emitted unconditionally.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/212_j4_tget_key_guard: skipped (no native target)")
  return
end

local function run (f, t, tbl, n)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.off()
  local want = f(t, tbl, n)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.on()
  local got = f(t, tbl, n)
  H.eq(got, want, "boolean key result")
end

-- Boolean payload 1 must not read t[1].
local q = 1
q = (q == 1)
local tbl = { b = q }
local t = { 888 }
run(function (tab, keys, n)
  local y
  for _ = 1, n do
    local k = keys.b
    y = tab[k]
    k = 1
  end
  return y
end, t, tbl, 100)
H.eq(t[1], 888, "integer entry untouched")

-- Float key must not read the entry keyed by its IEEE bits.
local bits = { [4609434218613702656] = 777 }
run(function (tab, _, n)
  local y
  for _ = 1, n do y = tab[1.5] end
  return y
end, bits, nil, 100)

-- A positive float key in a side trace too.
local side = {}
for i = 1, 200 do side[i] = i end
run(function (tab, _, n)
  local y
  for i = 1, n do y = tab[i + 0.0] end
  return y
end, side, nil, 100)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/212_j4_tget_key_guard ok")
