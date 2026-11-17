-- jit/09_while_fast.lua -- register-cached while prefix sums.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/09_while_fast: skipped (no native target)")
  return
end

local function w (n)
  local i, s = 0, 0
  while i < n do s = s + i; i = i + 1 end
  return s, i
end
local function wle (n)
  local i, s = 0, 0
  while i <= n do s = s + i; i = i + 1 end
  return s, i
end
local function wc ()
  local i, s = 0, 0
  while i < 100000 do s = s + i; i = i + 1 end
  return s, i
end
local function wsq (n)
  local i, s = 0, 0
  while i < n do s = s + i * i; i = i + 1 end
  return s, i
end
local function wf (n)
  local i, s = 0, 0.0
  while i < n do s = s + i * 0.5; i = i + 1 end
  return s, i
end
local function wstep (n)
  local i, s = 0, 0
  while i < n do s = s + i; i = i + 2 end
  return s, i
end
local function rev (n)
  local i, s = 0, 0
  while i < n do i = i + 1; s = s + i end
  return s, i
end

local a, ai = w(100000)
H.eq(a, 4999950000)
H.eq(ai, 100000)
local b, bi = wle(100000)
H.eq(b, 5000050000)
H.eq(bi, 100001)
local c, ci = wc()
H.eq(c, 4999950000)
H.eq(ci, 100000)
local ws, wsi = wsq(100000)
H.eq(ws, 333328333350000)
H.eq(wsi, 100000)
local wfa, wfi = wf(100000)
H.eq(wfa, 2499975000.0)
H.eq(wfi, 100000)
local wst, wsti = wstep(100000)
H.eq(wst, 2499950000)
H.eq(wsti, 100000)
local d = rev(100000)
H.eq(d, 5000050000)

jit.off()
H.eq(w(1000), 499500)
H.eq(wle(1000), 500500)
H.eq(rev(1000), 500500)
H.eq(wstep(1000), 249500)
H.eq(wf(1000), 249750.0)
H.eq(wsq(1000), 332833500)
jit.on()

print("jit/09_while_fast ok")
