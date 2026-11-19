-- jit/67_side_hash_insert.lua -- child/side insertion into an empty table.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/67_side_hash_insert: skipped (no native target)")
  return
end

local function f0 (x) return x + 1 end
local function f1 (x) return x + 2 end
local function g (f, t, n)
  local s = 0
  for i = 1, n do
    s = s + f(i)          -- root fails here when f changes
    t[1000 + i] = i       -- side inserts into the empty hash part
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local t1, t2 = {}, {}
H.eq(g(f0, t1, 1000), 1000 * 1001 / 2 + 1000)
H.eq(g(f1, t2, 1000), 1000 * 1001 / 2 + 2000)
for i = 1, 1000 do
  H.eq(rawget(t1, 1000 + i), i)
  H.eq(rawget(t2, 1000 + i), i)
end
H.check(jit.stats().ir_side_compiled >= 1, "hash-insert side not compiled")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/67_side_hash_insert ok")
