-- jit/44_prefix_fallback.lua -- prefixed table loops must stay correct.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/44_prefix_fallback: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("table=1")
jit.on()

local t = {}
for i = 1, 20001 do t[i] = i end
local function read (t, n)
  local s = 0
  for i = 1, n do
    local x = i + 1
    s = s + t[x] * 2
  end
  return s
end

H.eq(read(t, 20000), 400060000)

local u = table.new(0, 20001)
for i = 1, 20001 do u[i] = i end
H.eq(read(u, 20000), 400060000)

jit.flush()
jit.opt.start("table=0")
jit.on()
print("jit/44_prefix_fallback ok")
