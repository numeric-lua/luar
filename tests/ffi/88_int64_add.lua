-- tests/ffi/88_int64_add.lua -- fused int64 array element increment.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/88_int64_add: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/88_int64_add: skipped (no native target)")
  return
end

local n = 1001
local a = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do a[i] = i end
local function inc (m)
  for i = 0, m - 1 do a[i] = a[i] + 1 end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
inc(n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int64 add did not compile")
H.check(st.ir_executed >= 1, "int64 add did not execute")
H.eq(st.ir_entry_failures, 0)
H.eq(tostring(a[0]), "1LL")
H.eq(tostring(a[1000]), "1001LL")

-- Different source/destination arrays must not fuse as in-place add.
local b = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do b[i] = 10 end
local function cross (m)
  for i = 0, m - 1 do a[i] = b[i] + 1 end
end
jit.flush()
jit.opt.start("ir=1")
jit.on()
cross(n)
H.eq(tostring(a[0]), "11LL")
H.eq(tostring(a[1000]), "11LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
inc(n)
H.eq(tostring(a[1000]), "12LL")

jit.flush()
jit.opt.start("ir=0")
jit.on()
inc(n)
H.eq(tostring(a[1000]), "13LL")
print("ffi/88_int64_add ok")
