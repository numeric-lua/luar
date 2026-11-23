-- jit/153_while_multi_acc.lua -- a fast-while body may update its
-- accumulator only once.
--
-- The while/square/product peephole recognised each accumulating operation
-- independently.  A body such as `a = a + i*i; a = a*i` therefore matched
-- both the square and the product shapes; the emitter picked one of them
-- and silently computed a different loop.  The matcher must reject a second
-- accumulation and let the generic while decoder handle the body.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/153_while_multi_acc: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

local function check (name, body, n, init, binit)
  local src = string.format([[
    return function (n)
      local a, b, i = %s, %s, 1
      while i <= n do %s; i = i + 1 end
      return a, b
    end
  ]], init, binit or "3", body)
  local f = assert(load(src, "=jit153_" .. name))()
  jit.flush()
  jit.opt.start("hotloop=1")
  jit.off()
  local wa, wb = f(n)
  jit.flush()
  jit.opt.start("hotloop=1")
  jit.on()
  local ga, gb = f(n)
  H.check(same(ga, wa), name .. ": accumulator differs from the interpreter")
  H.check(same(gb, wb), name .. ": second accumulator differs")
end

-- The original miscompile: the product shape won and returned a constant.
check("square then product", "a = a + i*i; a = a*i", 57, "1")
check("sum then product", "a = a + i; a = a*i", 57, "1")
check("sub then product", "a = a - i; a = a*i", 57, "1")

-- A product followed by a sum must not be folded either.
check("product then sum", "a = a*i; a = a+i", 57, "1")

-- These single-accumulation shapes are still compiled by the peephole.
check("plain sum", "a = a + i", 300, "0")
check("square sum", "a = a + i*i", 300, "0")
check("plain product", "a = a * i", 30, "1")

H.check(jit.stats().compiled >= 1, "no loop compiled at all")

jit.opt.start("hotloop=56")
jit.flush()
print("jit/153_while_multi_acc ok")
