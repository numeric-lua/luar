-- ffi/168_const_expr_operators.lua -- C operators in enum values and array
-- sizes: comparisons, logical operators and the conditional operator.
local H = dofile("tests/helpers.lua")

if not pcall(require, "ffi") then
  print("ffi/168_const_expr_operators: skipped (FFI unavailable)")
  return
end

local ffi = require("ffi")

ffi.cdef[[
enum CE {
  CE_COND_TRUE  = 1 ? 2 : 3,
  CE_COND_FALSE = 0 ? 4 : 5,
  CE_CMP        = (1 < 2) ? 6 : 7,
  CE_EQ         = (2 == 3) || (4 != 5),
  CE_LOGIC      = (1 && 0) || 1,
  CE_LAZY_TRUE  = 1 ? 2 : 1 / 0,
  CE_LAZY_FALSE = 0 ? 1 / 0 : 3,
  CE_AND_LAZY   = 0 && (1 / 0),
  CE_OR_LAZY    = 1 || (1 / 0),
  CE_NESTED     = 1 ? (0 ? 8 : 9) : 10,
  CE_RIGHT      = 0 ? 11 : 0 ? 12 : 13,
  CE_PREC       = 1 + 2 << 1,
  CE_REL_EQ     = 1 < 2 == 1,
  CE_SHIFT_LAZY = 1 ? 14 : (1 << 64),
};
typedef char ce_arr[(1 ? 2 : 3)];
typedef char ce_arr2[(1 < 2) ? 4 : 5];
typedef char ce_arr3[(0 ? 6 : (2 == 2) ? 7 : 8)];
]]

H.eq(ffi.C.CE_COND_TRUE, 2)
H.eq(ffi.C.CE_COND_FALSE, 5)
H.eq(ffi.C.CE_CMP, 6)
H.eq(ffi.C.CE_EQ, 1)
H.eq(ffi.C.CE_LOGIC, 1)
H.eq(ffi.C.CE_LAZY_TRUE, 2)
H.eq(ffi.C.CE_LAZY_FALSE, 3)
H.eq(ffi.C.CE_AND_LAZY, 0)
H.eq(ffi.C.CE_OR_LAZY, 1)
H.eq(ffi.C.CE_NESTED, 9)
H.eq(ffi.C.CE_RIGHT, 13)
H.eq(ffi.C.CE_PREC, 6)
H.eq(ffi.C.CE_REL_EQ, 1)
H.eq(ffi.C.CE_SHIFT_LAZY, 14)
H.eq(ffi.sizeof("ce_arr"), 2)
H.eq(ffi.sizeof("ce_arr2"), 4)
H.eq(ffi.sizeof("ce_arr3"), 7)

-- Operators still combine with sizeof/offsetof and existing constants.
ffi.cdef[[
typedef struct CE_S { char a; int b; } CE_S;
enum { CE_SZ = sizeof(CE_S) + (1 ? 0 : 1),
       CE_OFF = offsetof(CE_S, b) == sizeof(int) ? 1 : 0 };
]]
H.eq(ffi.C.CE_SZ, ffi.sizeof("CE_S"))
H.eq(ffi.C.CE_OFF, 1)

-- Live errors are unchanged.
H.raises(function ()
  ffi.cdef("enum { CE_BAD_DIV = 1 / 0 };")
end, "division by zero")
H.raises(function ()
  ffi.cdef("enum { CE_BAD_SHIFT = 1 << 64 };")
end, "shift count out of range")
H.raises(function ()
  ffi.cdef("enum { CE_BAD_COLON = 1 ? 2 };")
end, "expected ':'")

print("ffi/168_const_expr_operators ok")
