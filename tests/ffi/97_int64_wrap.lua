-- tests/ffi/97_int64_wrap.lua -- fused int64 arithmetic is wrap-defined and
-- INT64_MIN / -1 (and % -1) do not trap.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/97_int64_wrap: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/97_int64_wrap: skipped (no native target)")
  return
end

local n = 1001
local imax = ffi.cast("int64_t", 0x7fffffffffffffff)
local imin = ffi.cast("int64_t", -9223372036854775807 - 1)

local function init (a)
  for i = 0, n - 1 do
    local m = i % 5
    if m == 0 then a[i] = imax
    elseif m == 1 then a[i] = imin
    elseif m == 2 then a[i] = imax - 1
    elseif m == 3 then a[i] = imin + 1
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

run("add 1", function (a)
  for i = 0, n - 1 do a[i] = a[i] + 1 end
end)
run("sub 1", function (a)
  for i = 0, n - 1 do a[i] = a[i] - 1 end
end)
run("add 100", function (a)
  for i = 0, n - 1 do a[i] = a[i] + 100 end
end)
run("mul 3", function (a)
  for i = 0, n - 1 do a[i] = a[i] * 3 end
end)
run("mul -2", function (a)
  for i = 0, n - 1 do a[i] = a[i] * -2 end
end)
run("idiv -1", function (a)
  for i = 0, n - 1 do a[i] = a[i] // -1 end
end)
run("mod -1", function (a)
  for i = 0, n - 1 do a[i] = a[i] % -1 end
end)
run("idiv 3", function (a)
  for i = 0, n - 1 do a[i] = a[i] // 3 end
end)
run("mod -2", function (a)
  for i = 0, n - 1 do a[i] = a[i] % -2 end
end)
run("idiv -1 alloc=0", function (a)
  for i = 0, n - 1 do a[i] = a[i] // -1 end
end, "alloc=0")

print("ffi/97_int64_wrap ok")
