-- jit/135_unary_minus.lua -- J2 compiles integer and float unary minus.
--
-- Integer negation must wrap at INT_MIN; float negation flips the IEEE sign
-- bit, so -0.0 and NaN payloads stay distinct from +0.0 and +NaN.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/135_unary_minus: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function check (name, src, ...)
  local args = table.pack(...)
  local ref = assert(load(src, "=unmref"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.off()
  local want = table.pack(ref(table.unpack(args, 1, args.n)))
  local f = assert(load(src, "=unmjit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local got = table.pack(f(table.unpack(args, 1, args.n)))
  local again = table.pack(f(table.unpack(args, 1, args.n)))
  for i = 1, want.n do
    if math.type(want[i]) == "float" then
      H.eq(bits(got[i]), bits(want[i]), name .. ": first call result " .. i)
      H.eq(bits(again[i]), bits(want[i]), name .. ": native call result " .. i)
    else
      H.eq(got[i], want[i], name .. ": first call result " .. i)
      H.eq(again[i], want[i], name .. ": native call result " .. i)
    end
  end
  H.check(jit.stats().compiled >= 1, name .. ": loop did not compile")
end

check("integer negation", [[
  return function (n)
    local s = 1
    for i = 1, n do s = -s end
    return s
  end
]], 1000)

check("integer index negation", [[
  return function (n)
    local s = 0
    for i = 1, n do s = s + (-i) end
    return s
  end
]], 1000)

check("INT_MIN negation", [[
  return function (n)
    local s = math.mininteger
    for i = 1, n do s = -s end
    return s
  end
]], 1000)

check("float negation", [[
  return function (n)
    local s = 1.5
    for i = 1, n do s = -s end
    return s
  end
]], 1000)

check("negative zero", [[
  return function (n)
    local s = 0.0
    for i = 1, n do s = -s end
    return s
  end
]], 1000)

check("NaN sign", [[
  return function (n)
    local s = 0 / 0
    for i = 1, n do s = -s end
    return s
  end
]], 1000)

check("branch negation", [[
  return function (n)
    local s = 0
    for i = 1, n do
      if (i & 1) ~= 0 then s = s + (-i) else s = s + i end
    end
    return s
  end
]], 1000)

check("while negation", [[
  return function (n)
    local s, i = 1.5, 1
    while i <= n do
      s = -s
      i = i + 1
    end
    return s
  end
]], 1000)

jit.flush()
print("jit/135_unary_minus ok")
