-- jit/73_flush_deferred.lua -- jit.flush() requested from a finalizer while
-- a native trace is running must not unmap the executing code.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/73_flush_deferred: skipped (no native target)")
  return
end

local flushed = 0
for i = 1, 500 do
  local garbage = setmetatable({ i }, {
    __gc = function ()
      flushed = flushed + 1
      jit.flush()
    end,
  })
  garbage = nil
end

local t = {}
jit.flush()
jit.opt.start("ir=1")
jit.on()
local function fill (tbl, n)
  for i = 1, n do tbl[i] = i end
end
-- Insertion grows the table and can step the GC; any finalizer that runs
-- here must defer freeing the code that is currently executing.
fill(t, 200000)
H.eq(#t, 200000)
H.eq(t[200000], 200000)
H.check(flushed >= 0, "finalizer counter corrupted")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/73_flush_deferred ok")
