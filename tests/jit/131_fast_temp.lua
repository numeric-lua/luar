-- jit/131_fast_temp.lua -- fast paths must still materialize intermediate
-- results in their bytecode register.
--
-- The specialised emitters compute a temporary expression (idx*idx,
-- idx*const, a table element or an inlined __index result) and fold it into
-- the accumulator.  The bytecode also writes that result to a register; if
-- the source keeps it in a local, the value must remain readable after the
-- loop.  Dropping the write left a stale local even though the sum matched.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/131_fast_temp: skipped (no native target)")
  return
end

local function run (name, src, ...)
  local args = table.pack(...)
  local ref = assert(load(src, "=tempref"))()
  jit.flush()
  jit.off()
  local want = table.pack(ref(table.unpack(args, 1, args.n)))
  local f = assert(load(src, "=tempjit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.opt.start("tint=1")
  jit.on()
  local got = table.pack(f(table.unpack(args, 1, args.n)))
  local again = table.pack(f(table.unpack(args, 1, args.n)))
  for i = 1, want.n do
    H.eq(got[i], want[i], name .. ": first call result " .. i)
    H.eq(again[i], want[i], name .. ": native call result " .. i)
  end
end

-- idx * idx into a local, integer and float accumulators
run("square sum", [[
  return function (n)
    local s, p = 0, 0
    for i = 1, n do
      p = i * i
      s = s + p
    end
    return s, p
  end
]], 1000)

run("square sum float acc", [[
  return function (n)
    local s, p = 0.0, 0
    for i = 1, n do
      p = i * i
      s = s + p
    end
    return s, p
  end
]], 1000)

-- idx * constant into a local (integer and float MULK)
run("mul constant", [[
  return function (n)
    local s, p = 0, 0
    for i = 1, n do
      p = i * 3
      s = s + p
    end
    return s, p
  end
]], 1000)

run("mul float constant", [[
  return function (n)
    local s, p = 0.0, 0.0
    for i = 1, n do
      p = i * 2.5
      s = s + p
    end
    return s, p
  end
]], 1000)

-- while loops: square and linear shapes
run("while square", [[
  return function (n)
    local i, s, p = 1, 0, 0
    while i <= n do
      p = i * i
      s = s + p
      i = i + 1
    end
    return s, p
  end
]], 1000)

run("while linear", [[
  return function (n)
    local i, s, p = 1, 0.0, 0.0
    while i <= n do
      p = i * 2.5
      s = s + p
      i = i + 1
    end
    return s, p
  end
]], 1000)

-- repeat loop with a square temporary
run("repeat square", [[
  return function (n)
    local i, s, p = 0, 0, 0
    repeat
      i = i + 1
      p = i * i
      s = s + p
    until i >= n
    return s, p
  end
]], 1000)

-- inlined __index result into a local
local mt = setmetatable({}, {__index = function (_, i) return i * 2 end})
run("meta result", [[
  return function (t, n)
    local s, v = 0, 0
    for i = 1, n do
      v = t[i]
      s = s + v
    end
    return s, v
  end
]], mt, 1000)

-- raw table element into a local
local arr = {}
for i = 1, 1000 do arr[i] = i end
run("table sum result", [[
  return function (t, n)
    local s, v = 0, 0
    for i = 1, n do
      v = t[i]
      s = s + v
    end
    return s, v
  end
]], arr, 1000)
H.check(jit.stats().compiled >= 1, "table sum result did not compile")

jit.flush()
print("jit/131_fast_temp ok")
