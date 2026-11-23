-- jit/152_branch_table_read.lua -- a branch body may contain the loop's one
-- raw array read.
--
-- J2 only decoded GETI/GETTABLE as the *first* instruction of a loop, so
-- `if cond then s = s + t[i] end` stayed interpreted even though the table
-- emitter already supported labelled code.  The read is still limited to a
-- single table and still exits on a metatable, a bad bound or a bad element
-- tag, all at the original bytecode PC.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/152_branch_table_read: skipped (no native target)")
  return
end

local function check (name, src, t, n)
  local f = assert(load(src, "=" .. name))()
  jit.flush()
  jit.opt.start("hotloop=1", "ir=0", "table=1", "tint=0")
  jit.off()
  local want = f(t, n)
  jit.flush()
  jit.on()
  local got = f(t, n)
  local again = f(t, n)
  H.eq(got, want, name .. ": first result")
  H.eq(again, want, name .. ": cached result")
  local st = jit.stats()
  H.check(st.compiled >= 1, name .. ": loop did not compile")
  H.check(st.sig_hits > 0, name .. ": loop never entered native code")
  H.eq(st.exits, 0, name .. ": loop exited its entry guard")
  return f
end

local t = {}
for i = 1, 1000 do t[i] = i end

check("gettable", [[
  return function (t, n)
    local s = 0
    for i = 1, n do
      if i % 2 == 0 then s = s + t[i] else s = s - 1 end
    end
    return s
  end
]], t, 200)

check("geti", [[
  return function (t, n)
    local s = 0
    for i = 1, n do
      if i % 2 == 0 then s = s + t[1] else s = s - 1 end
    end
    return s
  end
]], t, 200)

-- A metatable makes the native entry exit before it touches the array, and
-- the interpreter's __index result is used.
local meta = setmetatable({}, {__index = function () return 13 end})
local fm = check("metatable", [[
  return function (t, n)
    local s = 0
    for i = 1, n do
      if i % 2 == 0 then s = s + t[1] else s = s - 1 end
    end
    return s
  end
]], meta, 200)
H.eq(fm(meta, 200), 100 * 13 - 100, "metatable fallback result")

-- An out-of-bounds dynamic index exits at the GETTABLE PC and is completed
-- by the interpreter.  The interpreter then sees a nil element and raises
-- the normal arithmetic error; the failed native run must not swallow it.
local fs = check("out-of-bounds", [[
  return function (t, n)
    local s = 0
    for i = 1, n do
      if i % 2 == 0 then s = s + t[i] else s = s - 1 end
    end
    return s
  end
]], t, 200)
local short = {10, 20, 30}
H.raises(function () return fs(short, 200) end, "nil value")
H.eq(#short, 3, "out-of-bounds read changed the table")

jit.flush()
print("jit/152_branch_table_read ok")
