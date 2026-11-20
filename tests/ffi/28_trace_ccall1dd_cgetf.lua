-- tests/ffi/28_trace_ccall1dd_cgetf.lua -- cdata double reads feed CCALL1DD.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/28_trace_ccall1dd_cgetf: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/28_trace_ccall1dd_cgetf: skipped (no native target)")
  return
end

ffi.cdef[[
double fabs(double);
double ceil(double);
struct DBox { double *p; };
]]

local N = 1000
local d = ffi.new("double[?]", N)
local expected = 0.0
local expected_sum = 0.0
for i = 0, N - 1 do
  local v = (i % 2 == 0) and 2.5 or -1.5
  d[i] = v
  expected = expected + math.abs(v)
  expected_sum = expected_sum + v
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local fabs = ffi.C.fabs

-- CGETF: the call argument comes straight from a double array element.
local function sum_arr (fn, a, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(a[i]) end
  return s
end
H.near(sum_arr(fabs, d, N), expected)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "CGETF double C-call trace did not compile")
H.check(st.ir_executed >= 1, "CGETF double C-call trace did not execute")
H.eq(st.ir_call_exits, 0, "CGETF double C-call trace exited at the call")

-- CPGETF: the call argument comes from a pointer field element.
local box = ffi.new("struct DBox")
box.p = d
local function sum_pf (fn, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(b.p[i]) end
  return s
end
local compiled = st.ir_compiled
H.near(sum_pf(fabs, box, N), expected)
st = jit.stats()
H.check(st.ir_compiled > compiled, "CPGETF double C-call trace did not compile")
H.check(st.ir_executed >= 1, "CPGETF double C-call trace did not execute")
H.eq(st.ir_call_exits, 0, "CPGETF double C-call trace exited at the call")

-- Plain CGETF reads without a native C call must keep working.
local function sum_read (a, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] end
  return s
end
compiled = jit.stats().ir_compiled
H.near(sum_read(d, N), expected_sum)
st = jit.stats()
H.check(st.ir_compiled > compiled, "direct CGETF read trace did not compile")
H.check(st.ir_executed >= 1, "direct CGETF read trace did not execute")

-- Same Proto, but the cdata array is replaced by a Lua table of integers:
-- the cdata identity guard exits and the interpreter converts the ints.
local t = {}
for i = 0, N - 1 do t[i] = (i % 2 == 0) and 2 or -2 end
local call_exits = jit.stats().ir_call_exits
H.near(sum_arr(fabs, t, N), 2000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer table argument reached the double C-call op")

-- Function identity changes after the cdata read: the trace has already
-- stored the float argument, and the CALL-site guard must fall back to the
-- interpreter with both the new function and the argument slot consistent.
local expected_ceil = 0.0
for i = 0, N - 1 do expected_ceil = expected_ceil + math.ceil(d[i]) end
call_exits = jit.stats().ir_call_exits
H.near(sum_arr(ffi.C.ceil, d, N), expected_ceil)
H.eq(jit.stats().ir_call_exits, call_exits,
     "function change after a cdata read corrupted the call state")

-- Explicit int-to-double fallback on one Proto: float trace first, then an
-- integer argument must not be read as a double payload.
local function sum_val (fn, x, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(x) end
  return s
end
compiled = jit.stats().ir_compiled
H.near(sum_val(fabs, 2.5, N), 2500.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "double value trace did not compile")
call_exits = st.ir_call_exits
H.near(sum_val(fabs, 2, N), 2000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer argument reached the double C-call op")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/28_trace_ccall1dd_cgetf ok")
