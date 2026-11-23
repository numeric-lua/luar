-- jit/156_float_table_reads.lua -- constant-index float array elements.
--
-- OP_GETI can now specialize on a compile-time float array element.  The
-- per-iteration tag guard still compares the live tag, so an element that
-- changes flavour between calls falls back at the exact GETI PC.  A mixed
-- int/float J4 side trace must be rejected instead of executing integer
-- code on a live float accumulator.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/156_float_table_reads: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function check (source, ...)
  local chunk = assert(load("return function(n, ...) " .. source .. " end",
                            "=floatread"))
  local f = chunk()
  jit.flush()
  jit.off()
  local want = table.pack(f(...))
  jit.flush()
  jit.on()
  local got = table.pack(f(...))
  H.eq(got.n, want.n, "result count: " .. source)
  for i = 1, want.n do
    if math.type(want[i]) ~= math.type(got[i]) then
      error("result type differs at " .. i .. ": " .. source, 2)
    end
    if math.type(want[i]) == "float" then
      H.eq(bits(got[i]), bits(want[i]), "float bits differ at " .. i)
    else
      H.eq(got[i], want[i], "result " .. i .. ": " .. source)
    end
  end
  H.check(jit.stats().compiled >= 1, "loop did not compile: " .. source)
  H.check(jit.stats().sig_hits > 0, "native entry not taken: " .. source)
  return got
end

jit.opt.start("hotloop=1", "table=1", "tint=0", "ir=0")

-- Fused read + add, float and promoted-integer accumulators.
check([==[
  local t={2.5}
  local s=0.0
  for i=1,n do s=s+t[1] end
  return s
]==], 200)

check([==[
  local t={2.5}
  local s=0
  for i=1,n do s=s+t[1] end
  return s
]==], 200)

-- The bytecode result register keeps the element with its float tag.
check([==[
  local t={2.5}
  local s=0.0
  local x=0.0
  for i=1,n do x=t[1]; s=s+x end
  return s,x
]==], 200)

-- Constant-index float read-modify-write.
check([==[
  local t={1.5}
  for i=1,n do t[1]=t[1]+i end
  return t[1]
]==], 200)

-- Two distinct float tables, with the second read past the first fusion.
check([==[
  local a={1.5}
  local b={2.5}
  local s=0.0
  for i=1,n do s=s+a[1]+b[1] end
  return s
]==], 200)

-- NaN and infinity payloads are copied bit-for-bit.
check([==[
  local t={0/0}
  local s=0.0
  for i=1,n do s=s+t[1] end
  return s
]==], 10)

check([==[
  local t={1/0}
  local s=0.0
  for i=1,n do s=s+t[1] end
  return s
]==], 10)

-- The compiled float variant must reject an integer element at the GETI
-- guard, not reinterpret its payload.  A side trace is recorded on that
-- exit; the mixed int/float IR must not be emitted.
local chunk = assert(load([==[
  return function(t, n)
    local s=0.0
    for i=1,n do s=s+t[1] end
    return s, math.type(s)
  end
]==], "=flip"))
local flip = chunk()
jit.flush()
jit.on()
H.eq(flip({1.5}, 20), 30.0)
local before = jit.stats().ir_side_compiled
local s, ty = flip({2}, 20)
H.eq(s, 40.0, "mixed element flavour changed the result")
H.eq(ty, "float", "mixed element flavour changed the accumulator")
H.eq(jit.stats().ir_side_compiled, before,
     "mixed int/float side trace was emitted")

jit.opt.start("table=0")
jit.flush()
print("jit/156_float_table_reads ok")
