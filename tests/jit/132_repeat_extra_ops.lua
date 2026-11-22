-- jit/132_repeat_extra_ops.lua -- the specialised repeat emitter only runs
-- the increment and the accumulator operation, so it must only be selected
-- when the rest of the body is MMBIN/NOP placeholders.  A body with an
-- extra assignment used to be accepted and that assignment was dropped.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/132_repeat_extra_ops: skipped (no native target)")
  return
end

local function run (name, src, n)
  local ref = assert(load(src, "=repeatextraref"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.off()
  local want = table.pack(ref(n))
  local f = assert(load(src, "=repeatextrajit"))()
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local got = table.pack(f(n))
  local again = table.pack(f(n))
  for i = 1, want.n do
    H.eq(got[i], want[i], name .. ": first call result " .. i)
    H.eq(again[i], want[i], name .. ": native call result " .. i)
  end
  return again
end

-- A plain increment/accumulator operation after the extra statement.
run("extra after accumulator", [[
  return function (n)
    local i, s, t = 0, 0, 0
    repeat
      i = i + 1
      s = s + i
      t = t + 2
    until i >= n
    return s, t, i
  end
]], 1000)

-- Extra work before the matched increment.
run("extra before increment", [[
  return function (n)
    local i, s, t = 0, 0, 0
    repeat
      t = t + 3
      i = i + 1
      s = s - i
    until i >= n
    return s, t, i
  end
]], 1000)

-- A branch with its own comparison must not be folded away either.
run("extra branch", [[
  return function (n)
    local i, s, t = 0, 0, 0
    repeat
      i = i + 1
      s = s + i
      if i == 10 then t = 99 end
    until i >= n
    return s, t, i
  end
]], 1000)

-- The plain shape must still compile and run natively.
local pure = [[
  return function (n)
    local i, s = 0, 0
    repeat
      i = i + 1
      s = s + i
    until i >= n
    return s, i
  end
]]
local f = assert(load(pure, "=repeatpure"))()
jit.flush()
jit.opt.start("ir=0")
jit.on()
H.eq(f(1000), 500500)
H.eq(f(1000), 500500)
H.check(jit.stats().compiled >= 1, "plain repeat loop did not compile")

jit.flush()
print("jit/132_repeat_extra_ops ok")
