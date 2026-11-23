-- ffi/104_bool_conversions.lua -- C scalar-to-_Bool conversion is non-zero
-- to one, including fractional, infinite and NaN floats.
--
-- The old path converted through an integer first, so 0.5 and 1.5 became 0
-- and a _Bool bitfield stored the untruncated numeric value.  GCC assigns 1
-- to a _Bool for every non-zero scalar.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
_Bool helper_bool_not(_Bool b);
struct BoolBits { _Bool a:1; _Bool b:1; _Bool c; };
int size_BoolBits(void);
int boolbits_get_a(struct BoolBits *p);
int boolbits_get_b(struct BoolBits *p);
int boolbits_get_c(struct BoolBits *p);
void boolbits_set_a(struct BoolBits *p, _Bool v);
void boolbits_set_b(struct BoolBits *p, _Bool v);
void boolbits_set_c(struct BoolBits *p, _Bool v);
int boolbits_by_value(struct BoolBits p);
]]

local h = S.helper()

local function truth (v)
  local b = ffi.new("_Bool", v)
  local n = tonumber(b)
  H.check(n == 0 or n == 1, "a _Bool cdata must be 0 or 1, got " .. tostring(n))
  return n == 1
end

H.eq(truth(0), false, "integer zero")
H.eq(truth(1), true, "integer one")
H.eq(truth(2), true, "integer two")
H.eq(truth(-1), true, "negative integer")
H.eq(truth(0.0), false, "float zero")
H.eq(truth(-0.0), false, "negative zero")
H.eq(truth(0.5), true, "fractional float")
H.eq(truth(-0.5), true, "negative fractional float")
H.eq(truth(1.0 / 0.0), true, "positive infinity")
H.eq(truth(-1.0 / 0.0), true, "negative infinity")
H.eq(truth(0.0 / 0.0), true, "NaN")
H.eq(truth(ffi.new("double", 0.5)), true, "cdata double")
H.eq(truth(ffi.new("int", 0)), false, "cdata int zero")
H.eq(truth(ffi.new("int", -3)), true, "cdata int")
H.eq(truth(ffi.cast("void *", 0)), false, "NULL pointer")
H.eq(truth(ffi.cast("void *", 1)), true, "non-NULL pointer")
H.raises(function () ffi.new("_Bool", nil) end, "cannot convert")

H.eq(ffi.cast("_Bool", 0.5) ~= false, true, "cast fractional float")
H.eq(tonumber(ffi.cast("_Bool", 0.5)), 1, "cast fractional float value")
H.eq(tonumber(ffi.cast("_Bool", 0.0)), 0, "cast float zero value")

-- A _Bool argument crosses the C ABI with the same rule.
H.eq(h.helper_bool_not(0.5), false, "_Bool argument from a fraction")
H.eq(h.helper_bool_not(0), true, "_Bool argument from zero")
H.eq(h.helper_bool_not(ffi.new("double", -0.5)), false,
     "_Bool argument from cdata")

local b = ffi.new("struct BoolBits")
H.eq(ffi.sizeof("struct BoolBits"), h.size_BoolBits())
b.a = 1
b.b = 0
b.c = 0.5
H.eq(b.a, true)
H.eq(b.b, false)
H.eq(b.c, true)
H.eq(h.boolbits_get_a(b), 1)
H.eq(h.boolbits_get_b(b), 0)
H.eq(h.boolbits_get_c(b), 1)
H.eq(h.boolbits_by_value(b), 2)

-- Values outside 0/1 are normalised by the bitfield store.
b.a = 2
b.b = -1
b.c = 2
H.eq(h.boolbits_get_a(b), 1, "2 stored in a _Bool bitfield becomes 1")
H.eq(h.boolbits_get_b(b), 1, "-1 stored in a _Bool bitfield becomes 1")
H.eq(h.boolbits_get_c(b), 1, "2 stored in a _Bool field becomes 1")
H.eq(b.a, true)
H.eq(b.b, true)
H.eq(b.c, true)

-- The C setter writes the canonical 0/1 form.
h.boolbits_set_a(b, 0)
h.boolbits_set_b(b, 0)
h.boolbits_set_c(b, 0)
H.eq(b.a, false)
H.eq(b.b, false)
H.eq(b.c, false)

h.boolbits_set_a(b, 1)
h.boolbits_set_b(b, 1)
h.boolbits_set_c(b, 1)
H.eq(b.a, true)
H.eq(b.b, true)
H.eq(b.c, true)
H.eq(h.boolbits_by_value(b), 3)

print("ffi/104_bool_conversions ok")
