-- jit/153_seti_rk_store.lua -- array stores with immediate keys and RK values.
--
-- SETI stores an immediate array key with an RK(C) value, and SETTABLE also
-- takes RK(C).  The J2 decoder used to read R[C] unconditionally, so a
-- constant value could be replaced by whatever happened to live in that
-- register (for example a float local).  It also had no SETI support, so
-- t[1] = i stayed interpreted.  This covers both the correctness fix and the
-- new native store path.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/153_seti_rk_store: skipped (no native target)")
  return
end

local function run (source, ...)
  local chunk = assert(load("return function(n) " .. source .. " end", "=seti"))
  local f = chunk()
  jit.flush()
  jit.off()
  local want = table.pack(f(...))
  jit.flush()
  jit.on()
  local got = table.pack(f(...))
  return want, got
end

local function compare (want, got, source)
  H.eq(got.n, want.n, "result count: " .. source)
  for i = 1, want.n do
    if math.type(want[i]) ~= math.type(got[i]) then
      error("result type differs at " .. i .. ": " .. source, 2)
    end
    H.eq(got[i], want[i], "result " .. i .. ": " .. source)
  end
end

local function check (source, ...)
  local want, got = run(source, ...)
  compare(want, got, source)
  local st = jit.stats()
  H.check(st.compiled >= 1, "loop did not compile: " .. source)
  return got
end

local function check_interpreted (source, ...)
  local want, got = run(source, ...)
  compare(want, got, source)
  H.eq(jit.stats().compiled, 0, "loop unexpectedly compiled: " .. source)
  return got
end

jit.opt.start("hotloop=1", "table=1", "tint=0", "ir=0")

-- OP_SETI with a register value, integer and float.
check([==[local t={0} for i=1,n do t[1]=i end return t[1] ]==], 200)
check([==[local t={0.0} for i=1,n do t[1]=i*0.5 end return t[1] ]==], 200)

-- Float RK constants use the same store path; the key must survive while
-- the double bits are loaded into a register.
check([==[local t={0.0} for i=1,n do t[1]=1.5 end return t[1] ]==], 200)
check([==[
  local t={0.0,0.0,0.0,0.0}
  for i=1,n do t[i%4+1]=-2.25 end
  return t[1],t[2],t[3],t[4]
]==], 200)

-- A constant RK value must not be read from a same-numbered register.  x
-- occupies R1 and is a float; the constant 3 also has index 1 here.
check([==[
  local x=5.5
  local t={0}
  for i=1,n do t[1]=3 end
  return t[1], x
]==], 200)

-- Same shape through OP_SETTABLE with a computed key.
check([==[
  local x=5.5
  local t={0,0,0,0}
  for i=1,n do t[i%4+1]=3 end
  return t[1],t[2],t[3],t[4],x
]==], 200)

-- Float constants and a constant stored from inside a branch.
check([==[
  local x=5.5
  local t={0.0}
  for i=1,n do t[1]=2.5 end
  return t[1],x
]==], 200)
check([==[
  local x=5.5
  local t={0}
  for i=1,n do if i%2==0 then t[1]=7 end end
  return t[1],x
]==], 200)

-- A write past the array part exits to the interpreter and still lands in
-- the hash part; the native path must not grow the array itself.
check([==[local t={0} for i=1,n do t[2]=i end return t[1],t[2] ]==], 200)

-- A metatable disables the raw fast path; the interpreter's __newindex must
-- still be used.
check([==[
  local log=0
  local t=setmetatable({0},{__newindex=function(t,k,v) log=log+1; rawset(t,k,v) end})
  for i=1,n do t[1]=i end
  return t[1],log
]==], 200)

-- A hash-part immediate key is not an array store and stays interpreted.
check_interpreted([==[local t={0} for i=1,n do t[0]=i end return t[0] ]==], 200)

jit.opt.start("table=0")
jit.flush()
print("jit/153_seti_rk_store ok")
