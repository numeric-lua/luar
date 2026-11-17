-- ffi/14_array_struct.lua -- structs with fixed array members by value.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
struct Named { char name[8]; int n; };
int named_sum(struct Named x);
struct Named named_make(const char *s, int n);
struct PairBox { struct Named items[2]; };
int box_sum(struct PairBox b);
]]

local h = S.helper()
local x = ffi.new("struct Named")
x.name[0] = string.byte("a")
x.name[1] = string.byte("b")
x.n = 5
H.eq(h.named_sum(x), 7)

local y = ffi.new("struct Named", {"abc", 4})
H.eq(h.named_sum(y), 7)
local z = ffi.new("struct Named", {name = "abcd", n = 2})
H.eq(h.named_sum(z), 6)

local r = h.named_make("xy", 10)
H.eq(ffi.string(r.name), "xy")
H.eq(r.n, 10)
H.eq(h.named_sum(r), 12)

-- array members can also be arrays of structs
local b = ffi.new("struct PairBox")
b.items[0].n = 3
b.items[1].n = 4
H.eq(h.box_sum(b), 7)

print("ffi/14_array_struct ok")
