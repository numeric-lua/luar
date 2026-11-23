-- ffi/114_metatype_arith.lua -- metatype arithmetic, unary, bitwise and
-- concat dispatch, matching LuaJIT.
--
-- LuaJIT runs a struct/union metatype's arithmetic metamethods before
-- falling back to numeric cdata semantics.  Luar previously answered every
-- such operation with "invalid operands" and had no __concat at all.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef [[
typedef struct A { int n; } A;
typedef struct B { int n; } B;
typedef struct P { int n; } P;
]]

local seen = {}
local unary_args
local A = ffi.metatype("A", {
  __add = function (x, y) seen[#seen + 1] = "add"; return "A+" .. tostring(y) end,
  __sub = function (x, y) seen[#seen + 1] = "sub"; return "A-" end,
  __mul = function (x, y) seen[#seen + 1] = "mul"; return "A*" end,
  __div = function (x, y) seen[#seen + 1] = "div"; return "A/" end,
  __idiv = function (x, y) seen[#seen + 1] = "idiv"; return "A//" end,
  __mod = function (x, y) seen[#seen + 1] = "mod"; return "A%" end,
  __pow = function (x, y) seen[#seen + 1] = "pow"; return "A^" end,
  __band = function (x, y) seen[#seen + 1] = "band"; return "A&" end,
  __bor = function (x, y) seen[#seen + 1] = "bor"; return "A|" end,
  __bxor = function (x, y) seen[#seen + 1] = "bxor"; return "A~" end,
  __shl = function (x, y) seen[#seen + 1] = "shl"; return "A<<" end,
  __shr = function (x, y) seen[#seen + 1] = "shr"; return "A>>" end,
  __unm = function (x, y) seen[#seen + 1] = "unm"; unary_args = {x, y}; return "A-" end,
  __bnot = function (x, y) seen[#seen + 1] = "bnot"; return "~A" end,
  __concat = function (x, y) seen[#seen + 1] = "concat"; return "A.." end,
})

local B = ffi.metatype("B", {
  __add = function (x, y) seen[#seen + 1] = "badd"; return "B+" end,
  __concat = function (x, y) seen[#seen + 1] = "bconcat"; return "B.." end,
})

local a, a2, b = A(1), A(2), B(1)

-- Binary handlers receive the original operands, in either order.
H.eq(a + a2, "A+" .. tostring(a2), "__add not dispatched")
H.eq(a + 3, "A+3", "__add with a Lua number")
H.check((3 + a):sub(1, 2) == "A+", "__add with the cdata on the right")
H.eq(a - b, "A-", "__sub")
H.eq(a * b, "A*", "__mul")
H.eq(a / b, "A/", "__div")
H.eq(a // b, "A//", "__idiv")
H.eq(a % b, "A%", "__mod")
H.eq(a ^ b, "A^", "__pow")
H.eq(a & b, "A&", "__band")
H.eq(a | b, "A|", "__bor")
H.eq(a ~ b, "A~", "__bxor")
H.eq(a << b, "A<<", "__shl")
H.eq(a >> b, "A>>", "__shr")
H.eq(-a, "A-", "__unm")
H.eq(~a, "~A", "__bnot")
H.eq(a .. "x", "A..", "__concat on the left")
H.eq("x" .. a, "A..", "__concat on the right")
H.eq(a .. a2, "A..", "__concat two cdata")

-- The left operand's handler wins when both types define one.
H.eq(a + b, "A+" .. tostring(b), "left __add should win")
H.eq(b + a, "B+", "right type's __add")
H.eq(b .. a, "B..", "left __concat should win")
H.eq(a .. b, "A..", "left __concat should still win")

-- The unary handler sees the VM's duplicated operand pair.
H.eq(-a, "A-")
H.check(unary_args[1] == a and unary_args[2] == a,
        "unary __unm must receive the operand twice")

-- Numeric cdata has no metatype and keeps the FFI arithmetic paths.
local i = ffi.new("int64_t", 5)
local u = ffi.new("uint64_t", 7)
H.eq(tostring(i + 1), "6LL")
H.eq(tostring(u + 1), "8ULL")
H.eq(tostring(i // 2), "2LL")
H.eq(i < 6, true)

-- Without a handler the old clear errors remain.
local Plain = ffi.metatype("P", {})
local p = Plain(1)
H.raises(function () return p + 1 end, "invalid operands")
H.raises(function () return p .. "x" end, "concatenate")
H.raises(function () return -p end, "invalid operand")

-- Concatenation of ordinary scalars is unchanged.
H.eq(1 .. 2, "12")

-- Keep the A metatype from being replaced by the later U registration.
H.check(rawequal(A, ffi.typeof("A")), "type identity")
H.check(#seen > 0, "no metatype handler ran")

print("ffi/114_metatype_arith ok")
