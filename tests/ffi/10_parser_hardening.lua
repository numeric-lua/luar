-- ffi/10_parser_hardening.lua -- constant-expression and recursion edges.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

-- signed overflow in constant expressions wraps instead of trapping
ffi.cdef("enum { H_A = 1 << 62, H_B = H_A + H_A };")
H.eq(tonumber(ffi.C.H_B), math.mininteger)

-- INT64_MIN / -1 and % -1 do not raise SIGFPE
ffi.cdef("enum { H_D = (~9223372036854775807 - 1) / -1 };")
H.eq(tonumber(ffi.C.H_D), -9223372036854775807)
ffi.cdef("enum { H_M = (~9223372036854775807 - 1) % -1 };")
H.eq(tonumber(ffi.C.H_M), 0)

-- unary minus of the minimum integer wraps
ffi.cdef("enum { H_E = -(-9223372036854775807 - 1) };")
H.eq(tonumber(ffi.C.H_E), math.mininteger)

-- division by zero is a clean Lua error
H.raises(function () ffi.cdef("enum { H_Z = 1 / 0 };") end,
         "division by zero")
H.raises(function () ffi.cdef("enum { H_Z2 = 1 % 0 };") end,
         "division by zero")

-- deep declarator and constant-expression nesting is rejected, not a crash
local rep, sub = string.rep, string.sub
local deep = "int " .. rep("(", 5000) .. "x" .. rep(")", 5000) .. ";"
H.raises(function () ffi.cdef(deep) end, "nesting too deep")
local deepc = "enum { H_N = " .. rep("(", 5000) .. "1" .. rep(")", 5000) .. " };"
H.raises(function () ffi.cdef(deepc) end, "nested too deeply")

-- long type identifiers in function type ids must not overflow the id buffer
local tag = "HX" .. string.rep("x", 52)
ffi.cdef("struct " .. tag .. " { int a; }; struct " .. tag .. " hf(void);")
ffi.cdef("struct " .. tag .. " hg(int); struct " .. tag .. " hv;")
H.eq(ffi.sizeof("struct " .. tag), 4)

print("ffi/10_parser_hardening ok")
