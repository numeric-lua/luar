-- jit/154_table_rmw.lua -- read-modify-write array loops run natively.
--
-- The J2 table tier allowed one raw array read or one store, so the common
-- update shape t[k] = t[k] + x stayed interpreted.  A loop may now contain
-- one read and one write on the same table.  The exact interpreter fallback
-- must remain for holes, hash keys, out-of-range stores, metatables and
-- arithmetic errors.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/154_table_rmw: skipped (no native target)")
  return
end

local function run (source, ...)
  local chunk = assert(load("return function(n) " .. source .. " end", "=rmw"))
  local f = chunk()
  jit.flush()
  jit.off()
  local want = table.pack(pcall(f, ...))
  jit.flush()
  jit.on()
  local got = table.pack(pcall(f, ...))
  return want, got
end

local function same (want, got)
  if want.n ~= got.n then return false end
  for i = 1, want.n do
    if math.type(want[i]) ~= math.type(got[i]) then return false end
    if want[i] ~= got[i] then return false end
  end
  return true
end

local function check (source, n, native)
  local want, got = run(source, n)
  H.check(same(want, got), "results differ: " .. source)
  local st = jit.stats()
  if native then
    H.check(st.compiled >= 1, "loop did not compile: " .. source)
    H.check(st.sig_hits > 0, "native entry not taken: " .. source)
  end
  return got
end

jit.opt.start("hotloop=1", "table=1", "tint=0", "ir=0")

-- Immediate-key and computed-key read-modify-write, integer and float.
check([==[
  local t={0}
  for i=1,n do t[1]=t[1]+i end
  return t[1]
]==], 200, true)

check([==[
  local t={0,0,0,0}
  for i=1,n do local j=i%4+1; t[j]=t[j]+i end
  return t[1],t[2],t[3],t[4]
]==], 200, true)

check([==[
  local t={1,2,3,4}
  for i=1,n do local j=i%4+1; t[j]=t[j]*2 end
  return t[1],t[2],t[3],t[4]
]==], 60, true)

check([==[
  local t={0,0,0,0}
  for i=1,n do local j=i%4+1; t[j]=t[j]-1 end
  return t[1],t[2],t[3],t[4]
]==], 200, true)

-- The element is also kept in its bytecode result register.
check([==[
  local t={0,0,0,0}
  for i=1,n do local j=i%4+1; local v=t[j]; t[j]=v+1 end
  return t[1],t[2],t[3],t[4]
]==], 200, true)

-- Out-of-range read: the native path exits before the arithmetic, so the
-- interpreter raises the exact "nil" error.
local want, got = run([==[
  local t={}
  for i=1,n do t[1]=t[1]+i end
  return t[1]
]==], 50)
H.check(not want[1] and not got[1], "hole read should raise")
H.eq(got[2], want[2], "hole read error object differs")

-- Out-of-range store: the read and arithmetic run natively, then the store
-- exits to the interpreter and lands in the hash part.
check([==[
  local t={0}
  for i=1,n do t[2]=t[1]+i end
  return t[1],t[2]
]==], 200, true)

-- A metatable disables the raw path; __newindex must still run per store.
check([==[
  local nidx=0
  local t=setmetatable({0},{__newindex=function(t,k,v)
    nidx=nidx+1; rawset(t,k,v)
  end})
  for i=1,n do t[1]=t[1]+i end
  return t[1],nidx
]==], 200, false)

-- Division by zero must escape to the interpreter before the store.
local w2, g2 = run([==[
  local t={0}
  for i=1,n do t[1]=t[1]//0 end
  return t[1]
]==], 50)
H.check(not w2[1] and not g2[1], "zero division should raise")
H.eq(g2[2], w2[2], "zero division error object differs")

jit.opt.start("table=0")
jit.flush()
print("jit/154_table_rmw ok")
