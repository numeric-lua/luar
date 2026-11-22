-- tests/ffi/99_int64_unary.lua -- fused int64 unary minus and bitwise not.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/99_int64_unary: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/99_int64_unary: skipped (no native target)")
  return
end

local n = 1001
local imax = ffi.cast("int64_t", 0x7fffffffffffffff)
local imin = ffi.cast("int64_t", -9223372036854775807 - 1)

local function init (a)
  for i = 0, n - 1 do
    local m = i % 5
    if m == 0 then a[i] = imin
    elseif m == 1 then a[i] = imax
    elseif m == 2 then a[i] = -1
    elseif m == 3 then a[i] = 0
    else a[i] = i * 7 - 3000 end
  end
end

local function check_all (got, want, what)
  for i = 0, n - 1 do
    if tostring(got[i]) ~= tostring(want[i]) then
      error(string.format("%s: element %d got %s, want %s", what, i,
                          tostring(got[i]), tostring(want[i])), 2)
    end
  end
end

local function run (what, op, opts)
  local got = ffi.new("int64_t[?]", n)
  local want = ffi.new("int64_t[?]", n)
  init(got)
  init(want)

  jit.flush()
  if opts then
    jit.opt.start("ir=1", opts)
  else
    jit.opt.start("ir=1")
  end
  jit.on()
  op(got)
  local st = jit.stats()
  H.check(st.ir_compiled >= 1, what .. ": did not compile")
  H.check(st.ir_executed >= 1, what .. ": did not execute")
  H.eq(st.ir_entry_failures, 0, what .. ": entry guard failed")

  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  op(want)

  check_all(got, want, what)
end

run("unm", function (a)
  for i = 0, n - 1 do a[i] = -a[i] end
end)
run("bnot", function (a)
  for i = 0, n - 1 do a[i] = ~a[i] end
end)
run("unm alloc=0", function (a)
  for i = 0, n - 1 do a[i] = -a[i] end
end, "alloc=0")

-- uint64 unary ops produce the same bit pattern as the int64 path.
local u = ffi.new("uint64_t[?]", n)
local uw = ffi.new("uint64_t[?]", n)
for i = 0, n - 1 do
  u[i] = ffi.cast("uint64_t", -1) - i
  uw[i] = u[i]
end
local function uneg (a)
  for i = 0, n - 1 do a[i] = -a[i] end
end
jit.flush()
jit.opt.start("ir=1")
jit.on()
uneg(u)
local ust = jit.stats()
H.check(ust.ir_compiled >= 1, "uint64 unm did not compile")
H.check(ust.ir_executed >= 1, "uint64 unm did not execute")
jit.flush()
jit.opt.start("ir=0")
jit.on()
uneg(uw)
check_all(u, uw, "uint64 unm")

print("ffi/99_int64_unary ok")
