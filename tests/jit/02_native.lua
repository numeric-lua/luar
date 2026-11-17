-- jit/02_native.lua -- J2 native numeric for-loops.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/02_native: skipped (no native target on this host)")
  return
end

-- sum loop: the last iterations run natively after the hot threshold
local s = 0
for i = 1, 5000 do s = s + i end
H.eq(s, 12502500)
H.check(jit.stats().compiled >= 1, "no native loop compiled")
H.check(jit.stats().mcode > 0, "no machine code recorded")

-- ADDI body
local a = 0
for i = 1, 5000 do a = a + 2 end
H.eq(a, 10000)

-- descending and stepped loops
local d = 0
for i = 5000, 1, -1 do d = d + i end
H.eq(d, 12502500)
local q = 0
for i = 1, 5000, 7 do q = q + i end
H.eq(q, 1787500)

-- float arithmetic: MULK, mixed integer/float operands, DIV
local fx = 0.0
for i = 1, 5000 do fx = fx + i * 0.25 end
H.eq(fx, 3125625.0)
local mix = 0
for i = 1, 5000 do mix = mix + i * 0.5 end
H.eq(mix, 6251250.0)
local dv = 0.0
for i = 1, 5000 do dv = dv + i / 4.0 end
H.eq(dv, 3125625.0)

-- a type change makes the guards bail out and the interpreter takes over
local function scaled (a)
  local t = 0
  for i = 1, 5000 do t = t + a * i end
  return t
end
H.eq(scaled(2), 25005000)
H.eq(scaled(2.5), 31256250.0)

-- multiplication (small values, no overflow ambiguity)
local m = 1
for i = 1, 10 do m = m * 3 end
H.eq(m, 59049)

-- conditional bodies (integer comparisons and forward jumps)
local c = 0
for i = 1, 5000 do if i < 1000 then c = c + i end end
H.eq(c, 499500)
local e = 0
for i = 1, 5000 do
  if i <= 1000 then e = e + 1 else e = e + 2 end
end
H.eq(e, 9000)
local q = 0
for i = 1, 5000 do if i == 5000 then q = q + 10000 end end
H.eq(q, 10000)

-- while loops with integer conditions
local wi, ws = 0, 0
while wi < 100000 do ws = ws + wi; wi = wi + 1 end
H.eq(ws, 4999950000)
local wj, wv = 0, 0
while wj < 5000 do
  if wj == 2500 then wv = wv + 10000 end
  wj = wj + 1
end
H.eq(wv, 10000)

-- a for-loop break exits the native loop correctly
local fbs = 0
for i = 1, 5000 do
  if i == 100 then break end
  fbs = fbs + i
end
H.eq(fbs, 4950)

-- while break is conservative (interpreter) but correct
local wb, wbs = 0, 0
while wb < 5000 do
  wbs = wbs + wb
  wb = wb + 1
  if wb == 100 then break end
end
H.eq(wbs, 4950)

-- the compiled code must be abandoned when jit.off() is set
jit.off()
H.check(jit.status() == false, "status must be false after jit.off")
local off = 0
for i = 1, 5000 do off = off + i end
H.eq(off, 12502500)
jit.on()

-- flushing releases the code and future loops still interpret correctly
jit.flush()
H.eq(jit.stats().compiled, 0)
H.eq(jit.stats().mcode, 0)
local again = 0
for i = 1, 5000 do again = again + i end
H.eq(again, 12502500)

print("jit/02_native ok")
