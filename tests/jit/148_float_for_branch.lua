-- jit/148_float_for_branch.lua -- branches inside a float for-loop.
--
-- Branch loops add entry guards for the control triplet.  They used the
-- integer tag unconditionally, so a float-controlled loop with an 'if'
-- always failed its own entry guard.  The guard now follows for_float.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/148_float_for_branch: skipped (no native target)")
  return
end

local function lt (n)
  local s = 0.0
  for i = 1.0, n do
    if i < n * 0.5 then s = s + i else s = s - i end
  end
  return s
end

local function andor (n)
  local s = 0.0
  for i = 1.0, n do
    if i > 2.0 and i < n - 1.0 then s = s + i else s = s + 1.0 end
  end
  return s
end

local function ref_lt (n)
  local s = 0.0
  for i = 1.0, n do
    if i < n * 0.5 then s = s + i else s = s - i end
  end
  return s
end

local function ref_andor (n)
  local s = 0.0
  for i = 1.0, n do
    if i > 2.0 and i < n - 1.0 then s = s + i else s = s + 1.0 end
  end
  return s
end

jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=0")
jit.on()

local want, want2 = ref_lt(200), ref_andor(200)
H.eq(lt(200), want)
H.eq(lt(200), want)            -- cached native loop
H.eq(andor(200), want2)
H.eq(andor(200), want2)

H.check(jit.stats().compiled >= 2, "float branch loops did not compile")
H.check(jit.stats().sig_hits > 0, "float branch loop did not run natively")
H.eq(jit.stats().exits, 0, "float branch loop exited instead of running natively")

jit.flush()
print("jit/148_float_for_branch ok")
