-- jit/147_float_for_native.lua -- compiled float-controlled for loops run.
--
-- The VM called the recorder for a float FORLOOP but never handed the
-- backedge to the compiled code, so these loops were recorded and
-- "compiled" without executing natively.  sig_hits proves the cached
-- native entry was actually taken.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/147_float_for_native: skipped (no native target)")
  return
end

local function f (n)
  local s = 0.0
  for i = 1.0, n, 0.5 do s = s + i end
  return s
end

local function down (n)
  local s = 0.0
  for i = n, 1.0, -0.5 do s = s + i end
  return s
end

local function ref (n)
  local s = 0.0
  for i = 1.0, n, 0.5 do s = s + i end
  return s
end

local function refdown (n)
  local s = 0.0
  for i = n, 1.0, -0.5 do s = s + i end
  return s
end

jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=0")
jit.on()

local want, wantdown = ref(200), refdown(200)
H.eq(f(200), want)
H.eq(f(200), want)             -- cached native loop
H.eq(down(200), wantdown)
H.eq(down(200), wantdown)

H.check(jit.stats().compiled >= 1, "float for-loop did not compile")
H.check(jit.stats().sig_hits > 0, "float for-loop did not run natively")
H.eq(jit.stats().exits, 0, "float for-loop exited instead of running natively")

jit.flush()
print("jit/147_float_for_native ok")
