-- jit/128_bitwise_loops.lua -- J2 integer bitwise, unary not and constant
-- shift loops run natively and match the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/128_bitwise_loops: skipped (no native target)")
  return
end

local bodies = {
  "s = s & i",
  "s = s | i",
  "s = s ~ i",
  "s = s & 0x100000000",
  "s = s | 0x100000000",
  "s = s ~ 0x100000000",
  "s = ~s",
  "s = s << 3",
  "s = s >> 3",
  "s = s << 64",
  "s = s >> 64",
  "s = s << -1",
  "s = s >> -1",
  "if (i & 1) ~= 0 then s = s + i end",
  "if (i | 1) == i then s = s + 1 end",
}

local function check (name, src, n)
  local ref = assert(load(src, "=bitref"))()
  jit.flush()
  jit.off()
  local want = ref(n)
  local f = assert(load(src, "=bitjit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local first = f(n)
  local second = f(n)
  local st = jit.stats()
  jit.flush()
  jit.off()
  local was = f(n)
  H.eq(first, want, name .. ": first call")
  H.eq(second, want, name .. ": native call")
  H.eq(was, want, name .. ": interpreter")
  H.check(st.compiled >= 1, name .. ": loop did not compile")
end

for _, body in ipairs(bodies) do
  local start = body:find("~= 0") and "0" or "1"
  local src = "return function (n) local s = " .. start ..
              "; for i = 1, n do " .. body .. " end; return s end"
  check(body, src, 1000)
end

-- The same operations on a while loop.
local while_src = [[
  return function (n)
    local s, i = 0, 1
    while i <= n do
      s = (s | i) ~ (i & 7)
      i = i + 1
    end
    return s
  end
]]
check("while bitwise", while_src, 1000)

-- Integer-only guards: a float operand keeps the loop in the interpreter.
local float_src = [[
  return function (n)
    local s, i = 0, 1.0
    while i <= n do
      s = s + i
      i = i + 1
    end
    return s
  end
]]
jit.flush()
jit.off()
local fref = assert(load(float_src, "=floatref"))()
local fwant = fref(1000)
local ff = assert(load(float_src, "=floatjit"))()
jit.flush()
jit.on()
H.near(ff(1000), fwant)
H.near(ff(1000), fwant)

jit.flush()
print("jit/128_bitwise_loops ok")
