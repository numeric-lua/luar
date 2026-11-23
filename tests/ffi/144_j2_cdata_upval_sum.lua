-- ffi/144_j2_cdata_upval_sum.lua -- J2 reductions of cdata arrays captured
-- as closure upvalues.
--
-- The upvalue is re-resolved on every native entry, so rebinding it to
-- another array is observed exactly like the interpreter.  Strided and
-- descending loops use the same FORLOOP key sequence as the register-array
-- intrinsic.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/144_j2_cdata_upval_sum: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/144_j2_cdata_upval_sum: skipped (no native target)")
  return
end

local N = 257

local function make_sum ()
  local a
  local function set (v) a = v end
  local function f (n, start, stop, step)
    local s = 0
    for i = start, stop, step do s = s + a[i] end
    return s
  end
  return f, set
end

local function make (ct, fill)
  local a = ffi.new(ct .. "[?]", N)
  for i = 0, N - 1 do a[i] = fill(i) end
  return a
end

local function run (name, ct, fill, start, stop, step)
  local f, set = make_sum()
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  set(make(ct, fill))
  local want = f(N, start, stop, step)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  set(make(ct, fill))
  local before = jit.stats().tint_hits
  local got = f(N, start, stop, step)
  local hits = jit.stats().tint_hits - before
  H.eq(got, want, name .. ": sum")
  H.check(hits > 0, name .. ": did not use a cdata intrinsic")
end

run("int32 upvalue sum", "int32_t", function (i) return i * 3 - 5 end,
    0, N - 1, 1)
run("uint16 strided", "uint16_t", function (i) return i * 7 + 1 end,
    0, N - 1, 2)
run("int16 descending", "int16_t", function (i) return i - 10 end,
    N - 1, 0, -3)
run("double upvalue sum", "double", function (i) return i / 3 end,
    0, N - 1, 1)
run("float fraction", "float", function (i) return (i % 17) * 0.25 end,
    3, N - 1, 2)

-- Rebinding the upvalue after the loop has compiled must be observed.
do
  local f, set = make_sum()
  local a = make("int32_t", function (i) return i end)
  local b = make("int32_t", function (i) return 1000 - i end)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  set(a)
  local first = f(N, 0, N - 1, 1)
  H.eq(first, N * (N - 1) // 2, "upvalue sum first binding")
  set(b)
  local second = f(N, 0, N - 1, 1)
  local want = 0
  for i = 0, N - 1 do want = want + (1000 - i) end
  H.eq(second, want, "upvalue sum sees a rebound array")
end

-- A non-table, non-cdata upvalue falls back to the interpreter (or the
-- generic table path) instead of entering the cdata helper.
do
  local f, set = make_sum()
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  set(make("int32_t", function (i) return i end))
  f(N, 0, N - 1, 1)
  set({1, 2, 3})
  local ok2, err = pcall(f, N, 0, N - 1, 1)
  H.check(not ok2, "table upvalue should not run the cdata helper")
  H.check(tostring(err):find("nil value", 1, true) ~= nil,
          "unexpected table-upvalue error: " .. tostring(err))
end
