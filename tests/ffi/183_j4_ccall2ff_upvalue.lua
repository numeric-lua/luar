-- ffi/183_j4_ccall2ff_upvalue.lua -- J4 must not fold a mutable integer
-- upvalue into a C-call argument.
--
-- CCALL2FF used the record-time value of an integer ULOAD when converting
-- the float argument; changing the upvalue from 5 to 8 kept calling with 5.
-- The constant fold now only accepts LOADI constants, so the live upvalue
-- is read (and an ITOF conversion is emitted when needed).
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/183_j4_ccall2ff_upvalue: skipped (no native target)")
  return
end

ffi.cdef[[ float fmodf(float, float); ]]
local fmodf = ffi.C.fmodf

local k = 5
local function f ()
  local s = 0
  for _ = 1, 100 do s = s + fmodf(k, 2.0) end
  return s
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1")
jit.on()
H.eq(f(), 100, "first call")
k = 8
H.eq(f(), 0, "changed upvalue")
H.eq(f(), 0, "stable after change")
k = 5
H.eq(f(), 100, "restored upvalue")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("ffi/183_j4_ccall2ff_upvalue ok")
