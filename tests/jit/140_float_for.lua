-- jit/140_float_for.lua -- J2 compiles numeric for-loops whose control
-- values are floats.  FORPREP stores limit/step/index in R[A..A+2] for
-- float loops, and floatforloop() advances the index with IEEE addition and
-- NaN-aware comparisons; the native epilogue must match both exactly.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/140_float_for: skipped (no native target)")
  return
end

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

local function run (name, src, ...)
  local args = table.pack(...)
  local ref = assert(load(src, "=floatforref"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.off()
  local want = table.pack(ref(table.unpack(args, 1, args.n)))
  local f = assert(load(src, "=floatforjit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local got = table.pack(f(table.unpack(args, 1, args.n)))
  local again = table.pack(f(table.unpack(args, 1, args.n)))
  for i = 1, want.n do
    H.check(same(got[i], want[i]),
            name .. ": first call result " .. i .. " got " .. tostring(got[i]) ..
            " want " .. tostring(want[i]))
    H.check(same(again[i], want[i]),
            name .. ": native call result " .. i .. " got " .. tostring(again[i]) ..
            " want " .. tostring(want[i]))
  end
  return f
end

run("float init and integer limit", [[
  return function (n)
    local s = 0.0
    for i = 1.0, n do s = s + i end
    return s
  end
]], 200)

run("fractional step", [[
  return function (n)
    local s = 0.0
    for i = 1, n, 0.5 do s = s + i end
    return s
  end
]], 200)

run("fractional descending step", [[
  return function (n)
    local s = 0.0
    for i = n, 1.0, -0.5 do s = s + i end
    return s
  end
]], 200)

run("integer descending start", [[
  return function (n)
    local s = 0.0
    for i = n, 1.0, -1.0 do s = s + i * i end
    return s
  end
]], 200)

run("wide float step", [[
  return function (n)
    local s = 0.0
    for i = 1.0, n, 2.5 do s = s + i end
    return s
  end
]], 100)

run("float branch body", [[
  return function (n)
    local s = 0.0
    for i = 1.0, n do
      if i < n * 0.5 then s = s + i else s = s - i end
    end
    return s
  end
]], 200)

run("float temporary", [[
  return function (n)
    local s = 0.0
    local x
    for i = 1.0, n do
      x = i * 0.25
      s = s + x
    end
    return s, x
  end
]], 200)

-- NaN limit: FORPREP does not skip the loop (the NaN comparison is false),
-- one body runs, and the first floatforloop test is false.
run("NaN limit", [[
  return function ()
    local s = 0.0
    for i = 1.0, 0.0 / 0.0 do s = s + i end
    return s
  end
]])

-- NaN step: FORPREP treats the step as non-positive and skips the loop
-- because init < limit.
run("NaN step", [[
  return function ()
    local s = 7.0
    for i = 1.0, 10, 0.0 / 0.0 do s = s + i end
    return s
  end
]])

-- A float-controlled loop must be native, not a silent fallback.
local f = assert(load([[
  return function (n)
    local s = 0.0
    for i = 1.0, n, 0.5 do s = s + i end
    return s
  end
]], "=floatforcompile"))()
jit.flush()
jit.opt.start("ir=0")
jit.on()
H.eq(f(200), 40099.5)
H.eq(f(200), 40099.5)
H.check(jit.stats().compiled >= 1, "float for-loop did not compile")
H.eq(jit.stats().exits, 0, "float for-loop exited instead of running natively")

-- Integer-controlled loops still use the integer FORLOOP path.
jit.flush()
jit.opt.start("ir=0")
local g = assert(load([[
  return function (n)
    local s = 0
    for i = 1, n do s = s + i end
    return s
  end
]], "=intforcompile"))()
jit.on()
H.eq(g(200), 20100)
H.check(jit.stats().compiled >= 1, "integer for-loop regressed")

jit.flush()
print("jit/140_float_for ok")
