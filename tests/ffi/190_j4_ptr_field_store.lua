-- ffi/190_j4_ptr_field_store.lua -- J4 must execute a fused pointer-field
-- element write, not route CPSETI through the wrong helper.
--
-- The old emitter called luar_ffi_trace_ccall0_double with the store
-- arguments; the helper failed and the trace exited at the store.  The
-- visible value stayed correct, so check ir_executed as well.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/190_j4_ptr_field_store: skipped (no native target)")
  return
end

ffi.cdef[[ typedef struct { int *p; } PtrInt; ]]

local N = 200
local s = ffi.new("PtrInt")
local a = ffi.new("int[?]", N)
s.p = a

local function fill (st, n)
  for i = 0, n - 1 do st.p[i] = i end
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()
fill(s, N)
H.check(jit.stats().ir_compiled > 0, "pointer-field trace did not compile")
H.check(jit.stats().ir_executed > 0, "pointer-field store did not run natively")
for i = 0, N - 1 do H.eq(a[i], i, "element " .. i) end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("ffi/190_j4_ptr_field_store ok")
