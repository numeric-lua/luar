-- ffi/12_tls.lua -- __thread variables resolved through ffi.load.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

if ffi.os ~= "Linux" or ffi.arch ~= "x64" then
  print("ffi/12_tls: skipped (ELF TLS implementation)")
  return
end

ffi.cdef[[
extern __thread int ffi_tls_counter;
int ffi_tls_get(void);
void ffi_tls_set(int v);
]]

local h = S.helper()
H.eq(h.ffi_tls_get(), 11)
H.eq(h.ffi_tls_counter, 11)
h.ffi_tls_set(42)
H.eq(h.ffi_tls_get(), 42)
H.eq(h.ffi_tls_counter, 42)
h.ffi_tls_counter = 77
H.eq(h.ffi_tls_get(), 77)

print("ffi/12_tls ok")
