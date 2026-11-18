-- jit/16_repeat.lua -- repeat/until conditional backedges.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/16_repeat: skipped (no native target)")
  return
end

local function roff (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i until i >= n
  return s, i
end
local function ron (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i until i >= n
  return s, i
end
local function rstep (n)
  local i, s = 0, 0
  repeat i = i + 2; s = s + i until i >= n
  return s, i
end
local function rsub_off (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s - i until i >= n
  return s, i
end
local function rsub_on (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s - i until i >= n
  return s, i
end
local function rprod_off (n)
  local i, s = 0, 1
  repeat i = i + 1; s = s * i until i >= n
  return s, i
end
local function rprod_on (n)
  local i, s = 0, 1
  repeat i = i + 1; s = s * i until i >= n
  return s, i
end
local function rf_off (n)
  local i, s = 0, 0.0
  repeat i = i + 1; s = s + i until i >= n
  return s, i
end
local function rf_on (n)
  local i, s = 0, 0.0
  repeat i = i + 1; s = s + i until i >= n
  return s, i
end
local function rsq_off (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i * i until i >= n
  return s, i
end
local function rsq_on (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i * i until i >= n
  return s, i
end
local function rsqf_off (n)
  local i, s = 0, 0.0
  repeat i = i + 1; s = s + i * i until i >= n
  return s, i
end
local function rsqf_on (n)
  local i, s = 0, 0.0
  repeat i = i + 1; s = s + i * i until i >= n
  return s, i
end
local function rd_off (n)
  local i, s = 1, 1.0
  repeat s = s / i; i = i + 1 until i > n
  return s, i
end
local function rd_on (n)
  local i, s = 1, 1.0
  repeat s = s / i; i = i + 1 until i > n
  return s, i
end
local function rimm (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i until i >= 100000
  return s, i
end
local function rgt (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i until i > n
  return s, i
end
local function req (n)
  local i, s = 0, 0
  repeat i = i + 1; s = s + i until i == n
  return s, i
end

jit.off()
local want, wi = roff(100000)
local wstep = rstep(100000)
local wgt = rgt(100000)
local wreq = req(100000)
local wsub = rsub_off(100000)
local wprod = rprod_off(100)
local wf = rf_off(100000)
local wsq = rsq_off(100000)
local wsqf = rsqf_off(100000)
local wrd = rd_off(100)
jit.on()
local got, gi = ron(100000)
H.eq(got, want)
H.eq(gi, wi)
H.eq(rstep(100000), wstep)
H.eq(rgt(100000), wgt)
H.eq(req(100000), wreq)
H.eq(rimm(100000), want)
H.eq(rsub_on(100000), wsub)
H.eq(rprod_on(100), wprod)
H.eq(rf_on(100000), wf)
H.eq(rsq_on(100000), wsq)
H.eq(rsqf_on(100000), wsqf)
H.eq(rd_on(100), wrd)
H.check(jit.stats().compiled >= 1, "repeat loops did not compile")
H.check(jit.stats().traces >= 1, "repeat trace missing")

jit.off()
H.eq(ron(1000), roff(1000))
H.eq(rstep(1000), 250500)
jit.on()

print("jit/16_repeat ok")
