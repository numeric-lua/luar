-- jit/170_method_distinct_field.lua -- the method-add intrinsic must read
-- and write the same field of `self`.
--
-- The matcher used the GETFIELD key as the store key, so
-- `function o:add(x) self.w = self.v + x end` accumulated into self.v and
-- never updated self.w.  The intrinsic is only valid when GETFIELD and
-- SETFIELD use the same constant key and GETFIELD reads self.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/170_method_distinct_field: skipped (no native target)")
  return
end

local function run (src, n)
  local f = assert(load(src, "=methodfield"))()
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
  jit.off()
  local want = { pcall(f, n) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
  jit.on()
  local got = { pcall(f, n) }
  H.eq(#got, #want, "result count")
  H.eq(got[1], want[1], "error status")
  for i = 2, #want do
    H.eq(got[i], want[i], "result " .. i)
  end
  return got, jit.stats()
end

-- Distinct read/write fields must not use the single-field intrinsic.
local got = run([[
return function (n)
  local o = { v = 10, w = 99 }
  function o:add (x) self.w = self.v + x end
  for i = 1, n do o:add(i) end
  return o.v, o.w
end]], 300)
H.eq(got[2], 10, "read field was modified")
H.eq(got[3], 310, "written field was not updated")

-- Same-field methods still compile and keep their exact result.
local _, st = run([[
return function (n)
  local o = { v = 10, w = 99 }
  function o:add (x) self.v = self.v + x end
  for i = 1, n do o:add(i) end
  return o.v, o.w
end]], 300)
H.check(st.tint_hits >= 1, "same-field method did not use the intrinsic")

-- Reading another object (not self) stays on the interpreter path.
run([[
return function (n)
  local other = { v = 7 }
  local o = { v = 10, w = 99 }
  function o:add (x) self.w = other.v + x end
  for i = 1, n do o:add(i) end
  return o.v, o.w
end]], 300)

print("jit/170_method_distinct_field ok")
