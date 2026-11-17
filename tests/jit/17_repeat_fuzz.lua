-- jit/17_repeat_fuzz.lua -- differential repeat/until variants.
local H = dofile("tests/helpers.lua")

jit.on()
if jit.status() ~= true then
  print("jit/17_repeat_fuzz: skipped (no native target)")
  return
end

local seed = 0x1234abcd
local function rnd (n)
  seed = (seed * 1103515245 + 12345) & 0x7fffffff
  return seed % n
end

local function run (source)
  local f = assert(load(source, "@repeat-fuzz"))()
  return f()
end

for case = 1, 100 do
  local step = 1 + rnd(3)
  local k = 20 + rnd(40)
  local limit = step * k
  local op = ({">=", ">", "=="})[1 + rnd(3)]
  if op == " ==" then
    -- limit is a multiple of step, so equality terminates
  end
  local immediate = (case % 2 == 0)
  local init = (case % 3 == 0) and "0.0" or "0"
  local exprs = {"s + i", "s - i", "s * i", "s + i * i"}
  local expr = exprs[1 + (case % #exprs)]
  local cond = "i " .. op .. " " ..
               (immediate and tostring(limit) or "limit")
  local decl = immediate and "" or (" local limit = " .. limit)
  local src = ("return function()%s local i, s = 0, %s " ..
               "repeat i = i + %d; s = %s until %s " ..
               "return s, i end")
              :format(decl, init, step, expr, cond)
  jit.flush()
  jit.off()
  local want, wi = run(src)
  jit.on()
  local got, gi = run(src)
  H.eq(got, want, "repeat case " .. case)
  H.eq(gi, wi, "repeat index case " .. case)
end

jit.off()
print("jit/17_repeat_fuzz ok")
