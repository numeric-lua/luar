-- jit/176_j4_side_float_k.lua -- a J2 exit into a J4 side trace must not
-- reinterpret a float K constant as integer 0.
--
-- The root trace used to be integer-only, so ir_side_allows() skipped the
-- float analysis.  A side body such as `k * 0.5` was then emitted with the
-- integer MULK immediate (ival == 0) and retagged as an integer.  The side
-- trace ran the last iteration, so the interpreter never corrected it.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/176_j4_side_float_k: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end
local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    if a ~= a or b ~= b then return false end
    return bits(a) == bits(b)
  end
  return a == b
end

-- The constant key 5 lives in the hash part while key 1 fills the array, so
-- the table GETI exits into a side trace at the default hotloop.
local function make (expr)
  local src = string.format([[
    return function (t, n)
      local k = 7
      local y = 0
      for i = 1, n do
        local v = t[5]
        y = %s
      end
      return y
    end
  ]], expr)
  return assert(load(src, "=sidefloat"))
end

local exprs = {
  { "k * 0.5", "3.5" },  { "k + 0.5", "7.5" },  { "k - 0.5", "6.5" },
  { "k * 2.5", "17.5" }, { "k + 2.5", "9.5" },  { "k - 2.5", "4.5" },
  { "k // 2.5", "2.0" }, { "k % 2.5", "2.0" },
  { "t[5] * 1.0", "55.0" }, { "t[5] + 1.0", "56.0" },
  { "t[5] - 1.0", "54.0" },
}

for _, case in ipairs(exprs) do
  local f = make(case[1])()
  local t = { [1] = 1, [5] = 55 }
  jit.flush()
  jit.opt.start("table=1", "ir=0")
  jit.off()
  local want = f(t, 57)
  jit.flush()
  jit.opt.start("table=1", "ir=0")
  jit.on()
  local got = f(t, 57)
  H.check(same(got, want),
          case[1] .. ": got " .. tostring(got) .. ", want " .. tostring(want))
  H.eq(math.type(got), "float", case[1] .. ": result type")
end

-- An integer-only side trace still runs after the stricter validation.
do
  local f = make("k * 1")()
  local t = { [1] = 1, [5] = 55 }
  jit.flush()
  jit.opt.start("table=1", "ir=0")
  jit.off()
  H.eq(f(t, 57), 7, "integer side: interpreter")
  jit.flush()
  jit.opt.start("table=1", "ir=0")
  jit.on()
  H.eq(f(t, 57), 7, "integer side: jit")
  H.check(jit.stats().ir_side_executed >= 1,
          "integer side trace was not executed")
end

jit.flush()
jit.opt.start("table=0", "ir=0", "hotloop=56")
jit.on()
print("jit/176_j4_side_float_k ok")
