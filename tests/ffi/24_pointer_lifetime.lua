-- tests/ffi/24_pointer_lifetime.lua -- traced pointer fields need a rooted owner.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/24_pointer_lifetime: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[struct luar_lifetime { int *p; };]]
local n = 1000
local owner = ffi.new("int[?]", n)   -- retain the allocation
local s = ffi.new("struct luar_lifetime")
s.p = owner

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function read (s, n)
  local x = 0
  for i = 0, n - 1 do x = x + s.p[i] end
  return x
end
local function write (s, n)
  for i = 0, n - 1 do s.p[i] = i end
end
write(s, n)
collectgarbage(); collectgarbage()
H.eq(read(s, n), (n - 1) * n / 2)
H.eq(owner[n - 1], n - 1)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/24_pointer_lifetime ok")
