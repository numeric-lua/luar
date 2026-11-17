-- jit/05_while_fuzz.lua -- deterministic differential checks for while loops.
local H = dofile("tests/helpers.lua")

if jit.status() == false then
  print("jit/05_while_fuzz: skipped (no native target)")
  return
end

local state = 0x51c0ffee
local function rnd (n)
  state = state + 0x9e3779b97f4a7c15
  local z = state
  z = (z ~ (z >> 30)) * 0xbf58476d1ce4e5b9
  z = (z ~ (z >> 27)) * 0x94d049bb133111eb
  z = z ~ (z >> 31)
  return (z >> 33) % n + 1
end

local exprs = {
  "acc + i", "acc - i", "acc * i", "acc / i",
  "acc + i * 2", "acc * 2", "acc + 3", "i / 2", "acc + i * i",
}
local inits = { "0", "1", "7", "0.0" }

local function bits (x) return string.pack("d", x) end
local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

for case = 1, 100 do
  local expr = exprs[rnd(#exprs)]
  local init = inits[rnd(#inits)]
  local step = rnd(3)
  local n = 200 + rnd(300)
  local src = string.format([[
    return function (n)
      local i = 0
      local acc = %s
      while i < n do
        acc = %s
        i = i + %d
      end
      return acc, i
    end
  ]], init, expr, step)
  local chunk, err = load(src, "=whilefuzz" .. case)
  H.check(chunk ~= nil, err)
  local f = chunk()

  jit.flush()
  jit.on()
  local got_acc, got_i = f(n)
  jit.off()
  local want_acc, want_i = f(n)
  if not same(got_acc, want_acc) or got_i ~= want_i then
    error(string.format(
      "case %d (%s, init %s, step %d): jit (%s,%d) ~= interp (%s,%d)",
      case, expr, init, step, tostring(got_acc), got_i,
      tostring(want_acc), want_i), 2)
  end
end

jit.on()
print("jit/05_while_fuzz ok (100 cases)")
