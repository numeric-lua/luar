-- jit/173_fill_preserves_hash.lua -- growing the array part for a raw fill
-- must not discard the table's pre-existing hash part.
--
-- table_fill_intrinsic called luaH_resize(L, h, last, 0).  A zero hash size
-- made reinsertHash drop keys outside the new array (an integer key above
-- the fill range, a string key, a negative key) without raising.  The fill
-- now uses luaH_resizearray, which preserves the hash part.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/173_fill_preserves_hash: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
jit.on()

local function fill (t, n)
  for i = 1, n do t[i] = i end
end

local function make ()
  local t = {}
  t[1000] = "keep"
  t.key = "k"
  t[-1] = "neg"
  t[30] = "hash30"
  t.nested = { value = 7 }
  return t
end

local function snapshot (t)
  return {
    t[1000], t.key, t[-1], t[30], t.nested and t.nested.value,
    t[1], t[100], #t,
  }
end

-- Reference run with the interpreter.
jit.off()
local want_table = make()
fill(want_table, 100)
local want = snapshot(want_table)

-- The same input with the native fill.  A second call ensures the compiled
-- intrinsic, not just the recording pass, handles the remaining iterations.
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
jit.on()
local got_table = make()
fill(got_table, 100)
fill(got_table, 100)
local got = snapshot(got_table)

H.eq(#got, #want, "result count")
for i = 1, #want do
  H.eq(got[i], want[i], "fill changed pre-existing table entry " .. i)
end
H.check(jit.stats().tint_hits > 0, "the fill intrinsic did not run")

-- A pre-existing integer key just beyond the new array part must survive
-- the array growth too.
local edge = {}
edge[101] = "edge"
fill(edge, 100)
H.eq(edge[101], "edge", "hash key above the fill range was lost")
H.eq(edge[100], 100, "fill did not write the last array slot")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
print("jit/173_fill_preserves_hash ok")
