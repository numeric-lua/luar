-- jit/211_j4_tget_key_type.lua -- J4's dynamic GETTABLE helper accepts an
-- integer key.  Float/string/bool/table keys must not be passed as their
-- raw payload bits.
--
-- The side path performed a hash get with the bit pattern of a float key,
-- so t[1.5] could read an unrelated integer entry.  Non-integer keys now
-- stay on the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/211_j4_tget_key_type: skipped (no native target)")
  return
end

local function same (a, b)
  return math.type(a) == math.type(b) and a == b
end

local function run (name, f, t, expected)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.off()
  local want = f(t, 100)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.on()
  local got = f(t, 100)
  H.check(same(got, want), name .. ": " .. tostring(got) ..
          " ~= " .. tostring(want))
  H.check(same(got, expected), name .. ": expected " .. tostring(expected))
end

-- Raw IEEE bits of 1.5 must not become an integer table key.
local floatbits = { [4609434218613702656] = 777 }
run("float key", function (t, n)
  local y
  for _ = 1, n do y = t[1.5] end
  return y
end, floatbits, nil)

-- String, boolean and table keys take the interpreter path.
local sk = { x = 5 }
run("string key", function (t, n)
  local y
  for _ = 1, n do y = t["x"] end
  return y
end, sk, 5)

local bk = { [true] = 9 }
run("boolean key", function (t, n)
  local y
  for _ = 1, n do y = t[true] end
  return y
end, bk, 9)

local tk = {}
run("missing table key", function (t, n)
  local y
  for _ = 1, n do y = t[{}] end
  return y
end, tk, nil)

-- Integral float keys normalize to integers in the interpreter.
local ik = { [1] = 7 }
run("integral float key", function (t, n)
  local y
  for _ = 1, n do y = t[1.0] end
  return y
end, ik, 7)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/211_j4_tget_key_type ok")
