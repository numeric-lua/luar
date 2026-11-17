-- jit/12_product.lua -- register-cached product loops.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
if jit.status() ~= true then
  print("jit/12_product: skipped (no native target)")
  return
end

local function prod (n)
  local s = 1
  for i = 1, n do s = s * i end
  return s
end
local function prodf (n)
  local s = 1.0
  for i = 1, n do s = s * i end
  return s
end

local want, wantf
jit.off()
want = prod(15)
wantf = prodf(15)
jit.on()
for _ = 1, 10 do H.eq(prod(15), want) end
for _ = 1, 10 do H.eq(prodf(15), wantf) end
H.check(jit.stats().compiled >= 1, "product loop did not compile")

jit.off()
H.eq(prod(10), 3628800)
H.eq(prodf(10), 3628800.0)
jit.on()

print("jit/12_product ok")
