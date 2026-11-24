-- tests/ffi/98_int64_shift.lua -- fused int64 shifts use cdata's logical
-- shift and low-six-bit count rules.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/98_int64_shift: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/98_int64_shift: skipped (no native target)")
  return
end

local n = 1001
local imax = ffi.cast("int64_t", 0x7fffffffffffffff)
local imin = ffi.cast("int64_t", -9223372036854775807 - 1)

local function init (a)
  for i = 0, n - 1 do
    local m = i % 5
    if m == 0 then a[i] = -1
    elseif m == 1 then a[i] = imin
    elseif m == 2 then a[i] = imax
    elseif m == 3 then a[i] = -i
    else a[i] = i * 0x100000001 end
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
  jit.opt.start("ir=0", "tint=0")
  jit.on()
  op(want)

  check_all(got, want, what)
end

run("<<1", function (a)
  for i = 0, n - 1 do a[i] = a[i] << 1 end
end)
run(">>1", function (a)
  for i = 0, n - 1 do a[i] = a[i] >> 1 end
end)
run("<<7", function (a)
  for i = 0, n - 1 do a[i] = a[i] << 7 end
end)
run(">>7", function (a)
  for i = 0, n - 1 do a[i] = a[i] >> 7 end
end)
run("<<63", function (a)
  for i = 0, n - 1 do a[i] = a[i] << 63 end
end)
run(">>63", function (a)
  for i = 0, n - 1 do a[i] = a[i] >> 63 end
end)
run("<<64", function (a)
  for i = 0, n - 1 do a[i] = a[i] << 64 end
end)
run(">>64", function (a)
  for i = 0, n - 1 do a[i] = a[i] >> 64 end
end)
run("<<65", function (a)
  for i = 0, n - 1 do a[i] = a[i] << 65 end
end)
run(">>65", function (a)
  for i = 0, n - 1 do a[i] = a[i] >> 65 end
end)
run("<<-1", function (a)
  for i = 0, n - 1 do a[i] = a[i] << -1 end
end)
run(">>-1", function (a)
  for i = 0, n - 1 do a[i] = a[i] >> -1 end
end)
run("<<1 alloc=0", function (a)
  for i = 0, n - 1 do a[i] = a[i] << 1 end
end, "alloc=0")

print("ffi/98_int64_shift ok")
