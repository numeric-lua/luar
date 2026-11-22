-- jit/134_variable_shifts.lua -- J2 compiles register-count integer shifts.
--
-- Constant shifts were already handled through SHRI.  A count held in a
-- register compiles to OP_SHL/OP_SHR, whose Lua semantics are: a negative
-- count shifts the other way, and |count| >= 64 yields zero.  SHR is a
-- logical right shift.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/134_variable_shifts: skipped (no native target)")
  return
end

local function check (name, src, ...)
  local args = table.pack(...)
  local ref = assert(load(src, "=shiftref"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.off()
  local want = table.pack(ref(table.unpack(args, 1, args.n)))
  local f = assert(load(src, "=shiftjit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local got = table.pack(f(table.unpack(args, 1, args.n)))
  local again = table.pack(f(table.unpack(args, 1, args.n)))
  for i = 1, want.n do
    H.eq(got[i], want[i], name .. ": first call result " .. i)
    H.eq(again[i], want[i], name .. ": native call result " .. i)
  end
  H.check(jit.stats().compiled >= 1, name .. ": loop did not compile")
end

local shl = [[
  return function (n, k)
    local s = 1
    for i = 1, n do s = s << k end
    return s
  end
]]
local shr = [[
  return function (n, k)
    local s = -1
    for i = 1, n do s = s >> k end
    return s
  end
]]

for _, k in ipairs({0, 1, 7, 62, 63, 64, 65, -1, -7, -63, -64, -65,
                    math.mininteger, math.maxinteger}) do
  check("shl k=" .. tostring(k), shl, 200, k)
  check("shr k=" .. tostring(k), shr, 200, k)
end

-- Several shifts in one body, including a count produced by an operation.
check("multiple shifts", [[
  return function (n, k, j)
    local s = 1
    for i = 1, n do
      s = s << (i & 7)
      s = s >> j
      s = s << k
    end
    return s
  end
]], 500, 3, 2)

-- A branch keeps the comparison and the shifts in the labelled emitter.
check("branch shifts", [[
  return function (n, k)
    local s = 1
    for i = 1, n do
      if (i & 1) ~= 0 then
        s = s << k
      else
        s = s >> k
      end
    end
    return s
  end
]], 500, 5)

-- The while-loop emitter uses the same SHL/SHR body code.
check("while shifts", [[
  return function (n, k)
    local s, i = 1, 1
    while i <= n do
      s = s << k
      s = s >> 1
      i = i + 1
    end
    return s
  end
]], 500, 1)

jit.flush()
print("jit/134_variable_shifts ok")
