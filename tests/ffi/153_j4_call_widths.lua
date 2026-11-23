-- ffi/153_j4_call_widths.lua -- J4 scalar C-call return widths match the
-- interpreter.
--
-- A 64-bit integer result is cdata int64/uint64, not a Lua integer; the
-- scalar-call IR ops only materialise Lua integers, so those calls must
-- stay on the interpreter.  Narrow signed/unsigned integers and float32
-- results must be extended/converted with the declared type instead of
-- being read as a 64-bit or double payload.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/153_j4_call_widths: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")
if jit.status() ~= true then
  print("ffi/153_j4_call_widths: skipped (no native target)")
  return
end

local h = S.helper()

ffi.cdef[[
float trace_float_ret(void);
double trace_double_ret(void);
signed char trace_schar_ret(void);
unsigned int trace_uint_ret(void);
long labs(long);
]]

-- A fresh chunk gives every case its own Proto, so a trace compiled for
-- one C function is not reused for another.
local function make_runner (fn, init)
  local chunk = assert(load([[
    local fn, init = ...
    return function (n)
      local s = init
      for _ = 1, n do s = s + fn() end
      return s
    end
  ]], "=j4callwidth"))
  return chunk(fn, init)
end

local function check (name, fn, init, want_type, want_value, want_native)
  jit.flush()
  jit.opt.start("ir=0")
  jit.off()
  local want = make_runner(fn, init)(100)
  H.eq(math.type(want), want_type, name .. ": interpreter type")
  H.eq(want, want_value, name .. ": interpreter value")

  jit.flush()
  jit.opt.start("hotloop=1", "ir=1")
  jit.on()
  local before = jit.stats().ir_executed
  local got = make_runner(fn, init)(100)
  H.eq(math.type(got), want_type, name .. ": native type")
  H.eq(got, want_value, name .. ": native value")
  if want_native then
    H.check(jit.stats().ir_executed > before,
            name .. ": trace did not execute")
  end
end

-- Narrow integer results use the declared signedness.
check("signed char", h.trace_schar_ret, 0, "integer", -500, true)
check("unsigned int", h.trace_uint_ret, 0, "integer", 429496729500, true)

-- float(void) is 4 bytes and must be converted to a Lua double.
check("float", h.trace_float_ret, 0.0, "float", 125.0, true)
check("double", h.trace_double_ret, 0.0, "float", 250.0, true)

-- 64-bit C results remain cdata; the scalar IR call op must not replace
-- the interpreter's int64 object with a plain number.
do
  jit.flush()
  jit.opt.start("hotloop=1", "ir=1")
  jit.on()
  local chunk = assert(load([[
    local fn = ...
    return function (n)
      local s = 0
      for _ = 1, n do s = s + fn(-7) end
      return s
    end
  ]], "=j4callwidthlong"))
  local got = chunk(ffi.C.labs)(100)
  H.eq(type(got), "cdata", "long return type")
  H.eq(tostring(got), "700LL", "long return value")
end

jit.flush()
print("ffi/153_j4_call_widths ok")
