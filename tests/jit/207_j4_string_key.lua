-- jit/207_j4_string_key.lua -- J4 must not feed a non-integer key to the
-- integer-key TSET helper.
--
-- The generic TSET path takes a lua_Integer key; a string/float/bool key
-- was passed as its raw payload.  With a metatable (even an empty one) the
-- safe dynamic path is skipped, so a hot string-key store wrote a bogus
-- integer key and froze the real key at its last interpreted value.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/207_j4_string_key: skipped (no native target)")
  return
end

local function count (t)
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  return n
end

local function run (metatable, expected, expected_count)
  local t = metatable and setmetatable({}, metatable) or {}
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.on()
  local s = "key"
  for i = 1, 200 do t[s] = i end
  H.eq(t.key, expected, "string key value")
  H.eq(count(t), expected_count, "key count")
  H.eq(t[94342630018928], nil, "no payload-as-integer key")
end

run(nil, 200, 1)
run({}, 200, 1)                          -- metatable without __newindex
run({ __newindex = function () end }, nil, 0) -- handler: interpreter fallback

-- Float and boolean constant keys with a metatable also stay interpreted.
local t = setmetatable({}, {})
jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()
local k = 1.5
for i = 1, 200 do t[k] = i; t[true] = i end
H.eq(t[1.5], 200, "float key")
H.eq(t[true], 200, "boolean key")
H.eq(count(t), 2, "float/boolean key count")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/207_j4_string_key ok")
