-- jit/143_field_read.lua -- J2 folds a constant string-key field read and
-- guards its value at every native entry.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/143_field_read: skipped (no native target)")
  return
end

jit.flush()
jit.on()

local t = { x = 7, y = 2.5 }
local function fieldint (t, n)
  local s = 0
  for _ = 1, n do s = s + t.x end
  return s
end
local function fieldfloat (t, n)
  local s = 0.0
  for _ = 1, n do s = s + t.y end
  return s
end

H.eq(fieldint(t, 20000), 140000)
H.check(jit.stats().compiled >= 1, "integer field loop was not compiled")
H.eq(fieldfloat(t, 20000), 50000.0)
H.check(jit.stats().compiled >= 2, "float field loop was not compiled")

-- The entry value guard must reject a changed field instead of reusing the
-- stale constant.
t.x = 9
H.eq(fieldint(t, 20000), 180000)
H.check(jit.stats().exits >= 1, "field value change did not exit the trace")

t.y = -1.5
H.eq(fieldfloat(t, 2000), -3000.0)

-- After a flush the loop may compile again with the new value.
jit.flush()
jit.on()
H.eq(fieldint(t, 20000), 180000)
H.check(jit.stats().compiled >= 1, "field loop did not recompile after flush")

-- A local assigned inside the loop keeps the last field value.
local function last (t, n)
  local z = 0
  for _ = 1, n do z = t.x end
  return z
end
jit.flush()
jit.on()
H.eq(last(t, 2000), 9)

-- A present field is a raw lookup even with an __index metamethod.
local v = setmetatable({ z = 4 }, {
  __index = function () error("__index must not run for a present field") end,
})
local function meta (t, n)
  local s = 0
  for _ = 1, n do s = s + t.z end
  return s
end
jit.flush()
jit.on()
H.eq(meta(v, 2000), 8000)

-- A missing field must stay on the interpreter and use __index.
local u = setmetatable({}, { __index = function () return 3 end })
local function missing (t, n)
  local s = 0
  for _ = 1, n do s = s + t.z end
  return s
end
jit.flush()
jit.on()
H.eq(missing(u, 2000), 6000)
H.eq(jit.stats().compiled, 0, "missing field was folded instead of exiting")

print("jit/143_field_read ok")
