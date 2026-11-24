-- jit/210_j4_tgetk_miss.lua -- J4 string-key reads must not reuse a stale
-- result when the lookup misses.
--
-- luar_trace_tgetk (and the float/table/bool/string variants) left their
-- local TValue untouched on a miss, so a later miss reused the previous
-- field's value instead of exiting to __index.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/210_j4_tgetk_miss: skipped (no native target)")
  return
end

local function run (n)
  local t = setmetatable({ a = 5 }, {
    __index = function () return 100 end,
  })
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.off()
  local function f (tab, count)
    local s = 0
    for _ = 1, count do s = s + tab.a + tab.b end
    return s
  end
  local want = f(t, n)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.on()
  local got = f(t, n)
  H.eq(got, want, "string-key miss result")
end

run(100)
run(500)

-- A missing field after a hit across multiple string keys.
local t = setmetatable({ a = 1, b = 2 }, {
  __index = function (_, k) return #k end,
})
jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()
local function mixed (tab, n)
  local s = 0
  for _ = 1, n do s = s + tab.a + tab.b + tab.zz end
  return s
end
H.eq(mixed(t, 200), 200 * (1 + 2 + 2), "mixed string-key miss result")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/210_j4_tgetk_miss ok")
