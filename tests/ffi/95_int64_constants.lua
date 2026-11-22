-- tests/ffi/95_int64_constants.lua -- fused int64 ops keep full 64-bit
-- constant operands (no int/double truncation).
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/95_int64_constants: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/95_int64_constants: skipped (no native target)")
  return
end

local n = 1001

local function init (a)
  for i = 0, n - 1 do a[i] = i * 3 - 500 end
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

-- Constants above INT_MAX used to be truncated to int (or, for the K
-- operand, routed through a double).  All of these silently changed the
-- result before the fix.
run("mul 2^32", function (a)
  for i = 0, n - 1 do a[i] = a[i] * 4294967296 end
end)
run("mod 5e9", function (a)
  for i = 0, n - 1 do a[i] = a[i] % 5000000000 end
end)
run("idiv 5e9", function (a)
  for i = 0, n - 1 do a[i] = a[i] // 5000000000 end
end)
run("band 2^32", function (a)
  for i = 0, n - 1 do a[i] = a[i] & 4294967296 end
end)
run("bor 2^32", function (a)
  for i = 0, n - 1 do a[i] = a[i] | 4294967296 end
end)
run("bxor 2^32", function (a)
  for i = 0, n - 1 do a[i] = a[i] ~ 4294967296 end
end)
run("mul negative", function (a)
  for i = 0, n - 1 do a[i] = a[i] * -4294967296 end
end)

-- The alloc=0 path must carry the same constants.
run("mul 2^32 alloc=0", function (a)
  for i = 0, n - 1 do a[i] = a[i] * 4294967296 end
end, "alloc=0")

print("ffi/95_int64_constants ok")
