-- jit/139_shli.lua -- J2 compiles OP_SHLI (a constant shifted by a
-- register count).
--
-- Lua shifts by a negative count in the other direction and by |count| >= 64
-- to zero.  The constant-left form used to fall back to the interpreter even
-- though the register-left form was already native.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/139_shli: skipped (no native target)")
  return
end

local function run (name, src, ...)
  local args = table.pack(...)
  local ref = assert(load(src, "=shliref"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.off()
  local want = table.pack(ref(table.unpack(args, 1, args.n)))
  local f = assert(load(src, "=shlijit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local got = table.pack(f(table.unpack(args, 1, args.n)))
  local again = table.pack(f(table.unpack(args, 1, args.n)))
  for i = 1, want.n do
    H.eq(got[i], want[i], name .. ": first call result " .. i)
    H.eq(again[i], want[i], name .. ": native call result " .. i)
    H.eq(math.type(got[i]), "integer", name .. ": result type " .. i)
  end
  return f
end

run("positive count", [[
  return function (n)
    local s = 0
    for i = 1, n do s = s + (3 << (i & 63)) end
    return s
  end
]], 200)

run("variable count", [[
  return function (n, k)
    local s = 0
    for i = 1, n do s = s + (7 << k) end
    return s
  end
]], 100, 5)

for _, k in ipairs({0, 1, 7, 62, 63, 64, 65, -1, -2, -63, -64, -65,
                    math.mininteger, math.maxinteger}) do
  run("count " .. tostring(k), [[
    return function (n, k)
      local s = 0
      for i = 1, n do s = s + (5 << k) end
      return s
    end
  ]], 100, k)
end

run("negative constant", [[
  return function (n)
    local s = 0
    for i = 1, n do s = s + (-7 << (i & 63)) end
    return s
  end
]], 200)

run("while shift", [[
  return function (n, k)
    local s, i = 1, 1
    while i <= n do
      s = s ~ (3 << k)
      i = i + 1
    end
    return s
  end
]], 200, 9)

run("branch shift", [[
  return function (n, k)
    local s = 0
    for i = 1, n do
      if (i & 1) ~= 0 then s = s + (3 << k)
      else s = s - (3 << k) end
    end
    return s
  end
]], 200, 3)

-- A constant-left shift must actually compile natively.
local shli_src = [[
  return function (n)
    local s = 0
    for i = 1, n do s = s + (3 << (i & 63)) end
    return s
  end
]]
local ref = assert(load(shli_src, "=shliref"))()
jit.flush()
jit.opt.start("ir=0")
jit.off()
local want = ref(200)
local f = assert(load(shli_src, "=shlicompile"))()
jit.flush()
jit.opt.start("ir=0")
jit.on()
H.eq(f(200), want, "SHLI native result")
H.check(jit.stats().compiled >= 1, "SHLI loop did not compile")

jit.flush()
print("jit/139_shli ok")
