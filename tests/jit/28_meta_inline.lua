-- jit/28_meta_inline.lua -- inline a simple __index multiplier.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/28_meta_inline: skipped (no native target)")
  return
end

local function run (n)
  local o = setmetatable({}, {
    __index = function (_, k) return k * 2 end,
  })
  local s = 0
  for i = 1, n do s = s + o[i] end
  return s
end

jit.flush()
jit.on()
H.eq(run(10000), 100010000)
H.eq(run(10000), 100010000)
H.check(jit.stats().compiled >= 1, "metatable loop did not compile")

-- a non-empty table must bypass the missing-key fast path
local function run_nonempty (n)
  local o = setmetatable({ [1] = 10 }, {
    __index = function (_, k) return k * 2 end,
  })
  local s = 0
  for i = 1, n do s = s + o[i] end
  return s
end
H.eq(run_nonempty(100), 10 + 2 * (2 + 100) * 99 / 2)

-- a different multiplier is still valid
local function run3 (n)
  local o = setmetatable({}, {
    __index = function (_, k) return k * 3 end,
  })
  local s = 0
  for i = 1, n do s = s + o[i] end
  return s
end
H.eq(run3(1000), 3 * (1 + 1000) * 1000 / 2)

jit.off()
H.eq(run(100), 10100)
H.eq(run3(100), 15150)
jit.on()
print("jit/28_meta_inline ok")
