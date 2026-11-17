-- jit/04_fuzz.lua -- deterministic differential fuzzing of compiled loops.
--
-- Each case builds a fresh chunk (so it gets a fresh Proto), runs it with
-- the interpreter and then with the JIT, and compares value, math.type and
-- the exact double bits.  The generator is seeded, so failures reproduce.
local H = dofile("tests/helpers.lua")

if jit.status() == false then
  print("jit/04_fuzz: skipped (no native target)")
  return
end

local state = 0x1234abcd
local function rnd (n)
  state = state + 0x9e3779b97f4a7c15
  local z = state
  z = (z ~ (z >> 30)) * 0xbf58476d1ce4e5b9
  z = (z ~ (z >> 27)) * 0x94d049bb133111eb
  z = z ~ (z >> 31)
  return (z >> 33) % n + 1
end

local exprs = {
  "acc + i",
  "acc - i",
  "acc * i",
  "acc / i",
  "acc + i * 0.5",
  "acc * 0.5",
  "acc + 2",
  "acc * 3",
  "acc - 7",
  "i / 3",
  "acc + i / 4",
  "acc * (i + 1)",
}
local inits = { "0", "1", "0.0", "0.5" }

local function bits (x)
  return string.pack("d", x)
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

for case = 1, 150 do
  local expr = exprs[rnd(#exprs)]
  local init = inits[rnd(#inits)]
  local n = 250 + rnd(400)
  local src = string.format([[
    return function (n)
      local acc = %s
      for i = 1, n do
        acc = %s
      end
      return acc
    end
  ]], init, expr)
  local chunk, err = load(src, "=jitfuzz" .. case)
  H.check(chunk ~= nil, err)
  local f = chunk()

  jit.off()
  local want = f(n)
  jit.flush()
  jit.on()
  local got = f(n)
  if not same(got, want) then
    error(string.format("case %d (%s, init %s): jit %s (%s) ~= interp %s (%s)",
                        case, expr, init, tostring(got), tostring(math.type(got)),
                        tostring(want), tostring(math.type(want))), 2)
  end
end

print("jit/04_fuzz ok (150 cases)")
