-- ffi/159_j2_tint_retry.lua -- a transient intrinsic decline must not
-- permanently disable a compiled loop.
--
-- The dispatcher used to clear the intrinsic slot after the first runtime
-- refusal.  A single out-of-range cdata call (or a table whose metatable
-- makes the raw intrinsic decline) therefore turned every later call into
-- interpreted code even though the loop was native for normal inputs.
-- The slot is now suspended for the current call only.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/159_j2_tint_retry: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/159_j2_tint_retry: skipped (no native target)")
  return
end

local N = 257

-- cdata reduction: valid -> out-of-range -> valid.
do
  jit.flush()
  jit.opt.start("hotloop=1", "ir=0", "tint=1")
  jit.on()
  local function make ()
    local a = ffi.new("int32_t[?]", N)
    for i = 0, N - 1 do a[i] = i * 3 - 7 end
    return a
  end
  local f = assert(load([[
    return function (a, s, e, st)
      local r = 0
      for i = s, e, st do r = r + a[i] end
      return r
    end
  ]], "=tint_retry_cdata"))()

  local a = make()
  local want = f(a, 0, N - 1, 1)
  H.check(jit.stats().compiled >= 1, "cdata sum did not compile")
  H.check(jit.stats().tint_hits > 0, "cdata sum did not use the intrinsic")

  local b = make()
  H.raises(function () f(a, 0, N + 5, 1) end, "out of bounds")
  H.check(jit.stats().exits > 0, "out-of-range call did not decline")

  local before = jit.stats().tint_hits
  local c = make()
  H.eq(f(c, 0, N - 1, 1), want, "cdata sum after a transient miss")
  H.check(jit.stats().tint_hits > before,
          "transient miss permanently disabled the cdata intrinsic")
end

-- Raw table reduction: a metatable makes the intrinsic decline, but a raw
-- array read still succeeds on the interpreter; the next plain table must
-- use the native helper again.  Both calls are at the same stack depth, so
-- a stale CallInfo pointer would not be enough to tell them apart.
do
  jit.flush()
  jit.opt.start("hotloop=1", "ir=0", "table=0", "tint=1")
  jit.on()
  local function make (with_mt)
    local t = {}
    for i = 1, N do t[i] = i * 2 end
    if with_mt then
      setmetatable(t, { __index = function () return 0 end })
    end
    return t
  end
  local f = assert(load([[
    return function (t, n)
      local s = 0
      for i = 1, n do s = s + t[i] end
      return s
    end
  ]], "=tint_retry_table"))()

  local plain = make(false)
  local want = f(plain, N)
  H.check(jit.stats().tint_hits > 0, "table sum did not use the intrinsic")

  local decorated = make(true)
  H.eq(f(decorated, N), want, "metatable table sum")

  local before = jit.stats().tint_hits
  H.eq(f(plain, N), want, "table sum after a transient miss")
  H.check(jit.stats().tint_hits > before,
          "transient miss permanently disabled the table intrinsic")
end

jit.flush()
jit.opt.start("hotloop=56", "ir=0", "tint=1")
print("ffi/159_j2_tint_retry ok")
