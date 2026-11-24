-- jit/209_j4_tgeti_miss.lua -- J4 integer-key reads must not reuse a stale
-- result when the lookup misses.
--
-- luar_trace_tgeti left its local TValue untouched when luaH_getint found
-- no value, so a miss after a previous hit in the same trace reused the old
-- element instead of exiting to __index.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/209_j4_tgeti_miss: skipped (no native target)")
  return
end

local function run (n)
  local t = setmetatable({ [10] = 5 }, {
    __index = function (_, k) return 100 + k end,
  })
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.off()
  local function f (tab, count)
    local s = 0
    for _ = 1, count do s = s + tab[10] + tab[2] end
    return s
  end
  local want = f(t, n)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.on()
  local got = f(t, n)
  H.eq(got, want, "integer-key miss result")
end

run(100)
run(500)

-- A hit in the hash part followed by a miss in the array part.
local t = setmetatable({ [10] = 7 }, { __index = function () return 3 end })
jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()
local function mixed (tab, n)
  local s = 0
  for _ = 1, n do s = s + tab[10] + tab[2] end
  return s
end
H.eq(mixed(t, 200), 200 * 10, "mixed hit/miss result")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/209_j4_tgeti_miss ok")
