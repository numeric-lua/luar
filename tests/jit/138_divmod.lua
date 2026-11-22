-- jit/138_divmod.lua -- J2 compiles integer floor division and modulo.
--
-- Lua's `//` and `%` round towards negative infinity, unlike x86 IDIV.
-- The emitter adjusts the truncated quotient/remainder and exits to the
-- interpreter for division by zero and for the -1 special cases, so the
-- exact error messages and INT_MIN wrap behaviour stay those of T0.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/138_divmod: skipped (no native target)")
  return
end

local function run (name, src, ...)
  local args = table.pack(...)
  local ref = assert(load(src, "=divref"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.off()
  local want = table.pack(ref(table.unpack(args, 1, args.n)))
  local f = assert(load(src, "=divjit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local got = table.pack(f(table.unpack(args, 1, args.n)))
  local again = table.pack(f(table.unpack(args, 1, args.n)))
  for i = 1, want.n do
    H.eq(got[i], want[i], name .. ": first call result " .. i)
    H.eq(again[i], want[i], name .. ": native call result " .. i)
    H.eq(math.type(got[i]), math.type(want[i]),
         name .. ": first call type " .. i)
    H.eq(math.type(again[i]), math.type(want[i]),
         name .. ": native call type " .. i)
  end
  return f
end

local divf = run("integer floor division", [[
  return function (n)
    local s = 0
    for i = 1, n do s = s + (1000 // i) end
    return s
  end
]], 300)
H.check(jit.stats().compiled >= 1,
        "integer floor division did not compile")

run("negative floor division", [[
  return function (n)
    local s = -1000
    for i = 1, n do s = s // -i end
    return s
  end
]], 100)

run("integer modulo", [[
  return function (n)
    local s = -1000
    for i = 1, n do s = s % i end
    return s
  end
]], 200)

run("negative modulo", [[
  return function (n)
    local s = 1000
    for i = 1, n do s = s % -i end
    return s
  end
]], 200)

run("division and modulo together", [[
  return function (n)
    local q, r = 0, 0
    for i = 1, n do
      q = q + (i * 37 // (i + 3))
      r = r ~ (i % 7)
    end
    return q, r
  end
]], 300)

run("constant divisors", [[
  return function (n)
    local q, r = 0, 0
    for i = 1, n do
      q = q + i // 3
      r = r + i % -5
    end
    return q, r
  end
]], 300)

run("while division", [[
  return function (n)
    local s, i = 0, 1
    while i <= n do
      s = s + (i * i) // (i + 1)
      i = i + 1
    end
    return s
  end
]], 300)

run("branch division", [[
  return function (n)
    local s = 0
    for i = 1, n do
      if (i & 1) ~= 0 then s = s + i // 3 else s = s - i % 5 end
    end
    return s
  end
]], 300)

-- A divisor that becomes -1 must exit at the division PC; the interpreter
-- then applies the INT_MIN special case (and the exact wrap result).
run("divisor -1", [[
  return function (n)
    local s = 0
    local d = 1
    for i = 1, n do
      if i == 50 then d = -1 end
      s = s + (i // d)
    end
    return s
  end
]], 100)

run("INT_MIN divisor -1", [[
  return function (n)
    local s = math.mininteger
    local d = 1
    for i = 1, n do
      if i == 20 then d = -1 end
      s = s // d
    end
    return s
  end
]], 50)

-- A zero divisor still raises the interpreter's exact errors.
local zero_div = [[
  return function (n)
    local s = 0
    for i = 1, n do
      local d = i
      if i == 100 then d = 0 end
      s = s // d
    end
    return s
  end
]]
local zf = assert(load(zero_div, "=divzero"))()
jit.flush()
jit.opt.start("ir=0")
jit.on()
H.raises(function () zf(300) end, "attempt to divide by zero")
H.check(jit.stats().compiled >= 1, "zero-divisor // loop did not compile")

local zero_mod = [[
  return function (n)
    local s = 0
    for i = 1, n do
      local d = i
      if i == 100 then d = 0 end
      s = s % d
    end
    return s
  end
]]
local zm = assert(load(zero_mod, "=modzero"))()
jit.flush()
jit.opt.start("ir=0")
jit.on()
H.raises(function () zm(300) end, "attempt to perform 'n%%0'")
H.check(jit.stats().compiled >= 1, "zero-divisor %% loop did not compile")

-- Float operands stay on the interpreter and keep float `//`/`%` results.
run("float floor division", [[
  return function (n)
    local s = 0.0
    for i = 1, n do s = s + i // 2.5 end
    return s
  end
]], 200)

run("float modulo", [[
  return function (n)
    local s = 0.0
    for i = 1, n do s = s + i % 2.5 end
    return s
  end
]], 200)

jit.flush()
print("jit/138_divmod ok")
