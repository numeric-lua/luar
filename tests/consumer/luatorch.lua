-- consumer/luatorch.lua -- real-world FFI consumer from the numeric-lua stack.
--
-- Loads the C ABI of luatorch (libtorch-backed) with ffi.load and runs a few
-- tensor operations.  Skipped when the sibling luatorch checkout has not been
-- built, so the suite stays self-contained.

local okffi, ffi = pcall(require, "ffi")
if not okffi then
  print("consumer/luatorch: skipped (FFI not built)")
  return
end

local so = os.getenv("LUATORCH_SO")
if not so or so == "" then
  local candidates = {
    "../luatorch/build/libluatorch.so",
    "../../luatorch/build/libluatorch.so",
    "/home/machine/Códigos/numeric-lua/luatorch/build/libluatorch.so",
  }
  for _, p in ipairs(candidates) do
    local f = io.open(p, "rb")
    if f then f:close(); so = p; break end
  end
end
if not so then
  print("consumer/luatorch: skipped (libluatorch.so not built)")
  return
end

local ok, lib = pcall(ffi.load, so)
if not ok then
  print("consumer/luatorch: skipped (" .. tostring(lib) .. ")")
  return
end

ffi.cdef[[
typedef struct lt_tensor lt_tensor;
const char* lt_version(void);
const char* lt_last_error(void);
void lt_free(lt_tensor* t);
lt_tensor* lt_zeros(int64_t ndim, const int64_t* sizes, int dtype);
lt_tensor* lt_from_data(const void* data, int64_t ndim,
                        const int64_t* sizes, int dtype);
lt_tensor* lt_add(lt_tensor* a, lt_tensor* b, int inplace);
int64_t lt_dim(const lt_tensor* t);
int64_t lt_numel(const lt_tensor* t);
int lt_item_f64(const lt_tensor* t, int64_t ndim, const int64_t* index,
                double* out);
]]

local H = dofile("tests/helpers.lua")
local DTYPE_F64 = 1
local sizes = ffi.new("int64_t[2]", {2, 3})
local a = lib.lt_zeros(2, sizes, DTYPE_F64)
local b = lib.lt_zeros(2, sizes, DTYPE_F64)
H.check(a ~= nil and b ~= nil, ffi.string(lib.lt_last_error()))
H.eq(lib.lt_dim(a), 2)
H.eq(lib.lt_numel(a), 6)

local c = lib.lt_add(a, b, 0)
H.check(c ~= nil, ffi.string(lib.lt_last_error()))
local idx = ffi.new("int64_t[2]", {1, 2})
local out = ffi.new("double[1]")
H.eq(lib.lt_item_f64(c, 2, idx, out), 0)
H.eq(out[0], 0.0)

-- transfer a Lua-owned buffer into a tensor
local data = ffi.new("double[4]", {1.5, 2.5, 3.5, 4.5})
local one = ffi.new("int64_t[1]", {4})
local t = lib.lt_from_data(data, 1, one, DTYPE_F64)
H.check(t ~= nil, ffi.string(lib.lt_last_error()))
local i0 = ffi.new("int64_t[1]", {3})
H.eq(lib.lt_item_f64(t, 1, i0, out), 0)
H.eq(out[0], 4.5)

H.check(#ffi.string(lib.lt_version()) > 0)
lib.lt_free(a); lib.lt_free(b); lib.lt_free(c); lib.lt_free(t)
print("consumer/luatorch ok (" .. ffi.string(lib.lt_version()) .. ")")
