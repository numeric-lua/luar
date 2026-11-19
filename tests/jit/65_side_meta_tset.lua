-- jit/65_side_meta_tset.lua -- existing cells remain raw writes with a metatable.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/65_side_meta_tset: skipped (no native target)")
  return
end

local hits = 0
local t = setmetatable({}, {__newindex = function () hits = hits + 1 end})
for i = 1, 1000 do rawset(t, i, 0) end
local function f0 (x) return x + 1 end
local function f1 (x) return x + 2 end
local function g (f, t, n)
  local s = 0
  for i = 1, n do
    s = s + f(i)
    t[i] = i          -- existing array cell: no __newindex
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(g(f0, t, 1000), 1000 * 1001 / 2 + 1000)
H.eq(g(f1, t, 1000), 1000 * 1001 / 2 + 2000)
H.eq(hits, 0)
for i = 1, 1000 do H.eq(rawget(t, i), i) end

-- existing hash cell with the same metatable
local h = setmetatable({}, {__newindex = function () hits = hits + 1 end})
for i = 1001, 2000 do rawset(h, i, 0) end
local function hw (t, n)
  local s = 0
  for i = 1, n do
    t[1000 + i] = i
    s = s + t[1000 + i]
  end
  return s
end
H.eq(hw(h, 1000), 1000 * 1001 / 2)
H.eq(hits, 0)
for i = 1, 1000 do H.eq(rawget(h, 1000 + i), i) end

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/65_side_meta_tset ok")
