-- tests/ffi/96_uint64_divmod.lua -- fused int64 div/mod must use unsigned
-- semantics for uint64_t arrays.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/96_uint64_divmod: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/96_uint64_divmod: skipped (no native target)")
  return
end

local n = 1001
local umax = ffi.cast("uint64_t", -1)
local high = umax - 0x7fffffffffffffff   -- 0x8000000000000000

local function init (a)
  for i = 0, n - 1 do
    local m = i % 4
    if m == 0 then a[i] = umax
    elseif m == 1 then a[i] = umax - 1
    elseif m == 2 then a[i] = high
    else a[i] = i * 3 end
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
  local got = ffi.new("uint64_t[?]", n)
  local want = ffi.new("uint64_t[?]", n)
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

-- Before the fix these loops used signed int64 division/modulo after the
-- first interpreter iterations, so high-bit values silently changed.
run("uint64 //2", function (a)
  for i = 0, n - 1 do a[i] = a[i] // 2 end
end)
run("uint64 %3", function (a)
  for i = 0, n - 1 do a[i] = a[i] % 3 end
end)
run("uint64 //-1", function (a)
  for i = 0, n - 1 do a[i] = a[i] // -1 end
end)
run("uint64 %-1", function (a)
  for i = 0, n - 1 do a[i] = a[i] % -1 end
end)
run("uint64 //2 alloc=0", function (a)
  for i = 0, n - 1 do a[i] = a[i] // 2 end
end, "alloc=0")

-- Pointer indexing must select the same unsigned helper.
local raw = ffi.new("uint64_t[?]", n)
local pwant = ffi.new("uint64_t[?]", n)
init(raw)
init(pwant)
local p = ffi.cast("uint64_t *", raw)
jit.flush()
jit.opt.start("ir=1")
jit.on()
for i = 0, n - 1 do p[i] = p[i] // 2 end
local pst = jit.stats()
H.check(pst.ir_compiled >= 1, "unsigned pointer //2 did not compile")
H.check(pst.ir_executed >= 1, "unsigned pointer //2 did not execute")
H.eq(pst.ir_entry_failures, 0, "unsigned pointer //2 entry guard failed")
jit.flush()
jit.opt.start("ir=0")
jit.on()
for i = 0, n - 1 do pwant[i] = pwant[i] // 2 end
check_all(raw, pwant, "unsigned pointer //2")

print("ffi/96_uint64_divmod ok")
