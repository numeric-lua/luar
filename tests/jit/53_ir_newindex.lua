-- jit/53_ir_newindex.lua -- table IR must not bypass __newindex.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/53_ir_newindex: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

local hits = 0
local t = table.new(100, 0)
setmetatable(t, {
  __newindex = function (tab, k, v)
    hits = hits + 1
    rawset(tab, k, v)
  end,
})
local function fill (t, n)
  for i = 1, n do t[i] = i end
end
fill(t, 100)
H.eq(t[100], 100)
H.eq(hits, 100)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/53_ir_newindex ok")
