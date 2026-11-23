-- ffi/117_offsetof_bitfields.lua -- ffi.offsetof follows LuaJIT for packed
-- and full-width bitfields.
--
-- A packed bitfield wider than one byte is reported relative to its base
-- type's storage unit, not to the byte where the bit happens to start.
-- Fields that fill their base type are reported like ordinary members (a
-- single offset, with no bit position).  Narrow packed fields stay
-- byte-relative.  All forms are observable API, so keep them pinned.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef [[
typedef struct __attribute__((packed)) {
  char a;
  int b:12;
  int c:20;
} OffsetPackedA;

typedef struct __attribute__((packed)) {
  char pad[5];
  int b:12;
} OffsetPackedB;

typedef struct __attribute__((packed)) {
  char a;
  int b:8;
} OffsetPackedNarrow;

typedef struct __attribute__((packed)) {
  char a;
  unsigned b:3;
  unsigned c:3;
  unsigned d:3;
} OffsetPackedSpan;

typedef struct __attribute__((packed)) {
  char a;
  int b:32;
} OffsetPackedFull;

typedef struct {
  char a;
  int b:12;
  int c:20;
} OffsetPlain;

typedef struct {
  int b:32;
} OffsetPlainFull;

#pragma pack(push, 1)
struct OffsetPragmaPacked {
  char a;
  int b:12;
  int c:20;
};
#pragma pack(pop)
]]

local function packed (name, field, off, bit, width)
  local o, b, w = ffi.offsetof(name, field)
  H.eq(o, off, name .. "." .. field .. " byte offset")
  H.eq(b, bit, name .. "." .. field .. " bit position")
  H.eq(w, width, name .. "." .. field .. " width")
end

packed("OffsetPackedA", "b", 0, 8, 12)
packed("OffsetPackedA", "c", 0, 20, 20)
packed("OffsetPackedB", "b", 4, 8, 12)
packed("OffsetPackedNarrow", "b", 1, 0, 8)
packed("OffsetPackedSpan", "b", 1, 0, 3)
packed("OffsetPackedSpan", "c", 1, 3, 3)
packed("OffsetPackedSpan", "d", 1, 6, 3)
packed("struct OffsetPragmaPacked", "b", 0, 8, 12)
packed("struct OffsetPragmaPacked", "c", 0, 20, 20)

-- Unpacked bitfields keep the existing storage-unit form.
packed("OffsetPlain", "b", 0, 8, 12)
packed("OffsetPlain", "c", 4, 0, 20)

-- A bitfield that fills its base type is an ordinary member.
H.eq(select("#", ffi.offsetof("OffsetPackedFull", "b")), 1)
H.eq(ffi.offsetof("OffsetPackedFull", "b"), 1)
H.eq(select("#", ffi.offsetof("OffsetPlainFull", "b")), 1)
H.eq(ffi.offsetof("OffsetPlainFull", "b"), 0)

-- Non-bitfield members still return a single byte offset.
H.eq(select("#", ffi.offsetof("OffsetPackedA", "a")), 1)
H.eq(ffi.offsetof("OffsetPackedA", "a"), 0)
H.eq(select("#", ffi.offsetof("OffsetPackedB", "pad")), 1)
H.eq(ffi.offsetof("OffsetPackedB", "pad"), 0)

H.raises(function () ffi.offsetof("OffsetPackedA", "missing") end,
         "no member")

print("ffi/117_offsetof_bitfields ok")
