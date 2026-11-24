-- jit/195_j4_setfield_newindex.lua -- J4 constant-key stores must honor
-- __newindex for an absent field.
--
-- The TSETKK/TSETK/TSETKB/TSETKT helpers called luaH_set directly.  A hot
-- `t.x = i` loop over a table with __newindex therefore wrote the raw field
-- instead of calling the handler.  Existing raw fields still bypass
-- __newindex, as in the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/195_j4_setfield_newindex: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()

local function newtable ()
  local hits = 0
  local t = setmetatable({}, {
    __newindex = function (tab, k, v) hits = hits + 1 end,
  })
  return t, function () return hits end
end

local function loop_int (t, n)
  for i = 1, n do t.x = i end
end

local function loop_float (t, n)
  for i = 1, n do t.x = i + 0.5 end
end

local function loop_bool (t, n)
  for i = 1, n do t.x = (i % 2 == 0) end
end

local function loop_str (t, n)
  for i = 1, n do t.x = "s" .. i end
end

local function loop_table (t, n)
  local v = {}
  for i = 1, n do t.x = v end
end

local function loop_intkey (t, n)
  for i = 1, n do t[1] = i end
end

local cases = {
  {"integer", loop_int}, {"float", loop_float}, {"boolean", loop_bool},
  {"string", loop_str}, {"table", loop_table}, {"integer key", loop_intkey},
}

for _, c in ipairs(cases) do
  local name, f = c[1], c[2]
  local t, hits = newtable()
  f(t, 400)
  H.eq(t.x, nil, name .. ": __newindex should own the absent field")
  H.eq(t[1], nil, name .. ": integer key should stay absent")
  H.eq(hits(), 400, name .. ": __newindex call count")
end

-- An existing raw field bypasses __newindex in the interpreter and in J4.
local t2, hits2 = newtable()
rawset(t2, "x", 0)
loop_int(t2, 400)
H.eq(t2.x, 400, "existing field is written")
H.eq(hits2(), 0, "existing field bypasses __newindex")

-- A metatable without __newindex still allows the raw store.
local plain = setmetatable({}, { __index = function () return 0 end })
loop_int(plain, 400)
H.eq(plain.x, 400, "metatable without __newindex")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/195_j4_setfield_newindex ok")
