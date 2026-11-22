-- tests/ffi/86_int64_copy_guards.lua -- CCPYI64 may only fuse the exact
-- dst[k] = src[k] shape, must use the pointee address for pointer cdata, and
-- must resume before the load when the raw copy cannot run.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/86_int64_copy_guards: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/86_int64_copy_guards: skipped (no native target)")
  return
end

local n = 1001

local function run_ir (fn)
  jit.flush()
  jit.opt.start("ir=1")
  jit.on()
  return fn()
end

-- A pending int64 load must not swallow the arithmetic result.
local function additive ()
  local src = ffi.new("int64_t[?]", n)
  local dst = ffi.new("int64_t[?]", n)
  for i = 0, n - 1 do src[i] = i end
  jit.flush()
  local function f (m)
    for i = 0, m - 1 do dst[i] = src[i] + 1 end
  end
  f(n)
  local st = jit.stats()
  H.check(st.ir_aborts >= 1, "the recorder did not inspect src[i] + 1")
  H.eq(tostring(dst[0]), "1LL")
  H.eq(tostring(dst[500]), "501LL")
  H.eq(tostring(dst[1000]), "1001LL")
end

-- A discarded load must not turn `dst[i] = 0` into a copy.
local function discarded ()
  local src = ffi.new("int64_t[?]", n)
  local dst = ffi.new("int64_t[?]", n)
  for i = 0, n - 1 do src[i] = i + 1; dst[i] = 7 end
  jit.flush()
  local function f (m)
    for i = 0, m - 1 do
      local x = src[i]
      dst[i] = 0
    end
  end
  f(n)
  local st = jit.stats()
  H.check(st.ir_aborts >= 1, "the recorder did not inspect the discarded load")
  H.eq(tostring(dst[0]), "0LL")
  H.eq(tostring(dst[500]), "0LL")
  H.eq(tostring(dst[1000]), "0LL")
end

-- int64_t* cdata must index the pointee, not the cdata header.
local function pointers ()
  local src = ffi.new("int64_t[?]", n)
  local dst = ffi.new("int64_t[?]", n)
  for i = 0, n - 1 do src[i] = i + 1 end
  jit.flush()
  local sp = ffi.cast("int64_t*", src)
  local dp = ffi.cast("int64_t*", dst)
  local function f (m)
    for i = 0, m - 1 do dp[i] = sp[i] end
  end
  f(n)
  local st = jit.stats()
  H.check(st.ir_compiled >= 1, "pointer copy did not compile")
  H.check(st.ir_executed >= 1, "pointer copy did not execute")
  H.eq(tostring(dst[0]), "1LL")
  H.eq(tostring(dst[500]), "501LL")
  H.eq(tostring(dst[1000]), "1001LL")
end

-- A failed raw copy (source out of bounds) must resume at GETTABLE, so the
-- interpreter raises the normal error and leaves the rest of dst untouched.
local function bounds ()
  local ns, nd = 1001, 2001
  local src = ffi.new("int64_t[?]", ns)
  local dst = ffi.new("int64_t[?]", nd)
  for i = 0, ns - 1 do src[i] = i + 1 end
  for i = 0, nd - 1 do dst[i] = 77 end
  jit.flush()
  local function f (m)
    for i = 0, m - 1 do dst[i] = src[i] end
  end
  f(ns)                       -- install and execute the native trace
  H.check(jit.stats().ir_executed >= 1, "in-bounds copy did not execute")
  local ok, err = pcall(f, nd)
  H.check(not ok, "expected an out-of-bounds error")
  H.check(tostring(err):find("out of bounds"),
          "wrong error: " .. tostring(err))
  H.eq(tostring(dst[1000]), "1001LL")
  H.eq(tostring(dst[1001]), "77LL", "failed copy wrote stale data")
  H.eq(tostring(dst[1500]), "77LL", "failed copy wrote stale data")
end

-- A trace recorded over int64_t arrays is re-entered for double arrays that
-- share the same Proto; the helper must decline and the interpreter must
-- redo the load, not store whatever the load slot used to hold.
local function polymorphic ()
  local function make (src, dst)
    return function (m)
      for i = 0, m - 1 do dst[i] = src[i] end
    end
  end
  local si = ffi.new("int64_t[?]", n)
  local di = ffi.new("int64_t[?]", n)
  for i = 0, n - 1 do si[i] = i + 1 end
  local fi = make(si, di)
  fi(n)
  H.check(jit.stats().ir_executed >= 1, "int64 copy did not execute")
  local sd = ffi.new("double[?]", n)
  local dd = ffi.new("double[?]", n)
  for i = 0, n - 1 do sd[i] = i + 0.5 end
  local fd = make(sd, dd)
  fd(n)
  H.eq(tostring(di[500]), "501LL")
  H.near(dd[0], 0.5)
  H.near(dd[500], 500.5)
  H.near(dd[1000], 1000.5)
end

-- src == dst must stay a no-op through the raw copy.
local function self_copy ()
  local dst = ffi.new("int64_t[?]", n)
  for i = 0, n - 1 do dst[i] = i * 2 end
  local function f (m)
    for i = 0, m - 1 do dst[i] = dst[i] end
  end
  f(n)
  H.eq(tostring(dst[0]), "0LL")
  H.eq(tostring(dst[500]), "1000LL")
  H.eq(tostring(dst[1000]), "2000LL")
end

run_ir(additive)
run_ir(discarded)
run_ir(pointers)
run_ir(bounds)
run_ir(polymorphic)
run_ir(self_copy)
print("ffi/86_int64_copy_guards ok")
