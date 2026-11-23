-- jit/155_two_table_reads.lua -- two raw array reads per J2 loop.
--
-- The J2 table tier cached one table pointer and allowed one raw read, so
-- dot-product shapes such as s = s + a[i] + b[i] stayed interpreted.  A
-- loop may now read from two distinct tables (or the same table twice);
-- the second table gets its own table/array/asize state and entry/metatable
-- guards.  Three reads, writes combined with two reads, float elements and
-- out-of-range keys continue to fall back.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/155_two_table_reads: skipped (no native target)")
  return
end

local function run (source, ...)
  local chunk = assert(load("return function(n) " .. source .. " end", "=two"))
  local f = chunk()
  jit.flush()
  jit.off()
  local want = table.pack(pcall(f, ...))
  jit.flush()
  jit.on()
  local got = table.pack(pcall(f, ...))
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

local function check (source, n, native)
  local want, got = run(source, n)
  compare(want, got, source)
  local st = jit.stats()
  if native then
    H.check(st.compiled >= 1, "loop did not compile: " .. source)
    H.check(st.sig_hits > 0, "native entry not taken: " .. source)
  end
  return got
end

jit.opt.start("hotloop=1", "table=1", "tint=0", "ir=0")

-- Two distinct tables, with the element also used as a bytecode result.
check([==[
  local a={1,2,3}
  local b={4,5,6}
  local s=0
  for i=1,n do local j=i%3+1; s=s+a[j]+b[j] end
  return s
]==], 200, true)

check([==[
  local a={1,2,3}
  local b={4,5,6}
  local s=0
  local x=0
  for i=1,n do local j=i%3+1; x=a[j]; s=s+x+b[j] end
  return s,x
]==], 200, true)

-- Two reads from the same table use the first table's cached state.
check([==[
  local a={1,2,3}
  local s=0
  for i=1,n do s=s+a[1]+a[2] end
  return s
]==], 200, true)

-- The second read may sit inside a branch.
check([==[
  local a={1,2,3}
  local b={4,5,6}
  local s=0
  for i=1,n do if i%2==0 then s=s+a[1]+b[1] end end
  return s
]==], 200, true)

-- A second table with a metatable is not raw-accessible: the native entry
-- guard must leave the whole loop to the interpreter.
check([==[
  local a={1,2,3}
  local b=setmetatable({4,5,6},{__index=function() return 0 end})
  local s=0
  for i=1,n do local j=i%3+1; s=s+a[j]+b[j] end
  return s
]==], 200, false)

-- Out-of-range access on the second table raises exactly as the
-- interpreter does.
local want, got = run([==[
  local a={1,2,3}
  local b={}
  local s=0
  for i=1,n do local j=i%3+1; s=s+a[j]+b[j] end
  return s
]==], 50)
H.check(not want[1] and not got[1], "second-table hole should raise")
H.eq(got[2], want[2], "second-table error object differs")

-- Three reads stay interpreted rather than overwriting a cached state.
check([==[
  local a={1,2,3}
  local b={4,5,6}
  local s=0
  for i=1,n do s=s+a[1]+b[1]+a[2] end
  return s
]==], 200, false)

-- Float array elements are outside the integer J2 fast path.
check([==[
  local a={1.5,2.5}
  local b={4.5,5.5}
  local s=0.0
  for i=1,n do local j=i%2+1; s=s+a[j]+b[j] end
  return s
]==], 200, false)

jit.opt.start("table=0")
jit.flush()
print("jit/155_two_table_reads ok")
