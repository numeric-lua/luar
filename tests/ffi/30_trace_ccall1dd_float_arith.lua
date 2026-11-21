-- tests/ffi/30_trace_ccall1dd_float_arith.lua
-- Float arithmetic on cdata reads feeds CCALL1DD.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/30_trace_ccall1dd_float_arith: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/30_trace_ccall1dd_float_arith: skipped (no native target)")
  return
end

ffi.cdef[[
double fabs(double);
double ceil(double);
struct FloatBox { double x; double *p; };
]]

local N = 1000
local d = ffi.new("double[?]", N)
local e = ffi.new("double[?]", N)
for i = 0, N - 1 do
  local v
  if i % 4 == 0 then v = 2.5
  elseif i % 4 == 1 then v = -1.75
  elseif i % 4 == 2 then v = 0.5
  else v = -3.25 end
  d[i] = v
  e[i] = i * 0.5 - 2.0
end
local box = ffi.new("struct FloatBox")
box.x = 3.5
box.p = d
local fabs, ceil = ffi.C.fabs, ffi.C.ceil

jit.flush()
jit.opt.start("ir=1")
jit.on()

local function compiled_ok (before, name)
  local st = jit.stats()
  H.check(st.ir_compiled > before, name .. " did not compile")
  H.check(st.ir_executed >= 1, name .. " did not execute")
  H.eq(st.ir_call_exits, 0, name .. " exited at the call")
end

-- MULK with a float constant.
local function sum_mulf (fn, a, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(a[i] * 0.5) end
  return s
end
local want = 0.0
for i = 0, N - 1 do want = want + math.abs(d[i] * 0.5) end
local before = jit.stats().ir_compiled
H.near(sum_mulf(fabs, d, N), want)
compiled_ok(before, "MULK float")

-- MULK with an integer constant on a float producer.
local function sum_muli (fn, a, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(a[i] * 2) end
  return s
end
want = 0.0
for i = 0, N - 1 do want = want + math.abs(d[i] * 2) end
before = jit.stats().ir_compiled
H.near(sum_muli(fabs, d, N), want)
compiled_ok(before, "MULK integer")

-- DIVK always yields a float, even with an integer constant.
local function sum_div (fn, a, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(a[i] / 2) end
  return s
end
want = 0.0
for i = 0, N - 1 do want = want + math.abs(d[i] / 2) end
before = jit.stats().ir_compiled
H.near(sum_div(fabs, d, N), want)
compiled_ok(before, "DIVK")

-- ADD of two cdata double reads.
local function sum_add (fn, a, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(a[i] + b[i]) end
  return s
end
want = 0.0
for i = 0, N - 1 do want = want + math.abs(d[i] + e[i]) end
before = jit.stats().ir_compiled
H.near(sum_add(fabs, d, e, N), want)
compiled_ok(before, "ADD")

-- SUB of two cdata double reads.
local function sum_sub (fn, a, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(a[i] - b[i]) end
  return s
end
want = 0.0
for i = 0, N - 1 do want = want + math.abs(d[i] - e[i]) end
before = jit.stats().ir_compiled
H.near(sum_sub(fabs, d, e, N), want)
compiled_ok(before, "SUB")

-- The same proof works through a pointer field (CPGETF).
local function sum_pmul (fn, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(b.p[i] * 2) end
  return s
end
want = 0.0
for i = 0, N - 1 do want = want + math.abs(d[i] * 2) end
before = jit.stats().ir_compiled
H.near(sum_pmul(fabs, box, N), want)
compiled_ok(before, "CPGETF MULK")

-- And through a scalar struct field (CGETFIELD, double).
local function sum_fmul (fn, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fn(b.x * 2) end
  return s
end
before = jit.stats().ir_compiled
H.near(sum_fmul(fabs, box, N), math.abs(box.x * 2) * N)
compiled_ok(before, "CGETFIELD MULK")

-- Integer values on the same Proto must not reach the double call op.
local t = {}
for i = 0, N - 1 do t[i] = (i % 2 == 0) and 2 or -2 end
want = 0.0
for i = 0, N - 1 do want = want + math.abs(t[i] * 2) end
local call_exits = jit.stats().ir_call_exits
H.near(sum_muli(fabs, t, N), want)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer table reached the double C-call op")

-- Function identity changes after the float expression: the CALL-site guard
-- must fall back with the argument slot consistent.
want = 0.0
for i = 0, N - 1 do want = want + math.ceil(d[i] + e[i]) end
call_exits = jit.stats().ir_call_exits
H.near(sum_add(ceil, d, e, N), want)
H.eq(jit.stats().ir_call_exits, call_exits,
     "function change after float arithmetic corrupted the call state")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/30_trace_ccall1dd_float_arith ok")
