-- jit/14_fast_fuzz.lua -- differential fuzzing of the register fast paths.
local H = dofile("tests/helpers.lua")

jit.on()
if jit.status() ~= true then
  print("jit/14_fast_fuzz: skipped (no native target)")
  return
end

local seed = 0x5eed1234
local function rnd (n)
  seed = (seed * 1103515245 + 12345) & 0x7fffffff
  return seed % n
end

local function make (n, body)
  local src = "return function(n) " .. body .. " end"
  local f = assert(load(src, "@fuzz"))
  return f(), src
end

for case = 1, 120 do
  local kind = case % 9
  local step = 1 + rnd(3)
  local konst = 1 + rnd(7)
  local body, n
  n = 70 + rnd(40)
  if kind == 0 then
    body = "local s=0 for i=1,n do s=s+i end return s"
  elseif kind == 1 then
    body = "local s=0 for i=1,n do s=s+i*i end return s"
  elseif kind == 2 then
    body = "local s=1 for i=1,n do s=s*i end return s"
  elseif kind == 3 then
    body = "local s=0 for i=1,n do s=s+i*" .. konst .. " end return s"
  elseif kind == 4 then
    body = "local s=0.0 for i=1,n do s=s+i*0.5 end return s"
  elseif kind == 5 then
    body = "local i,s=0,0 while i<n do s=s+i i=i+1 end return s"
  elseif kind == 6 then
    body = "local i,s=0,0 while i<n do s=s+i*i i=i+1 end return s"
  elseif kind == 7 then
    body = "local i,s=1,1 while i<=n do s=s*i i=i+1 end return s"
  else
    body = "local i,s=0,0 while i<n do s=s+i i=i+" .. step .. " end return s"
  end
  local f, src = make(n, body)
  jit.flush()
  jit.off()
  local want = f(n)
  jit.on()
  local got = f(n)
  H.eq(got, want, "case " .. case .. " differs: " .. src)
end

jit.on()
print("jit/14_fast_fuzz ok")
