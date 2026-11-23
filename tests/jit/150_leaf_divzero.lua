-- jit/150_leaf_divzero.lua -- inlined leaf calls must not hide a division by
-- zero.
--
-- leaf_eval() used jit_idiv(), whose zero-divisor result was a plain zero.
-- A leaf intrinsic therefore returned a value where the interpreter raises
-- 'attempt to divide by zero'; the first ten loop iterations stay below the
-- branch, so the zero divisor is reached only inside the native run.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/150_leaf_divzero: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=0", "call=1")
jit.on()

local function is_zero_error (err)
  local s = tostring(err)
  return s:find("zero") ~= nil or s:find("n%%0") ~= nil
end

local function check (name, src)
  local f = assert(load(src, "=" .. name))()
  local function loop (n, d, fn)
    local acc = 0
    for i = 1, n do acc = acc + fn(i, d) end
    return acc
  end
  local warm = loop(100, 2, f)
  H.check(warm > 0, name .. ": warm loop produced no sum")
  jit.flush()
  jit.opt.start("hotloop=1", "ir=0", "table=0", "call=1")
  jit.on()
  H.eq(loop(100, 2, f), warm, name .. ": warm result differs")
  H.check(jit.stats().tint_hits > 0, name .. ": leaf intrinsic did not run")

  local ok, err = pcall(loop, 100, 0, f)
  H.check(not ok, name .. ": zero divisor returned a value")
  H.check(is_zero_error(err),
          name .. ": wrong error: " .. tostring(err))

  jit.off()
  local ok2, err2 = pcall(loop, 100, 0, f)
  H.check(not ok2, name .. ": interpreter accepted zero divisor")
  H.check(is_zero_error(err2),
          name .. ": interpreter error changed: " .. tostring(err2))
  jit.on()
end

check("idiv", [[
  return function (i, d)
    if i > 10 then return i // d end
    return i
  end
]])

check("mod", [[
  return function (i, d)
    if i > 10 then return i % d end
    return i
  end
]])

jit.flush()
print("jit/150_leaf_divzero ok")
