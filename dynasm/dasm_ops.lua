------------------------------------------------------------------------------
-- dasm_ops.lua -- 32-bit operation helpers for DynASM on Lua 5.5+.
--
-- DynASM was written for LuaJIT and used the Lua 5.1-era `bit` library.
-- LuaR ships no such compatibility library: its backends run on native Lua
-- 5.5 operators instead.  This module provides the exact 32-bit semantics
-- the encoders expect (signed int32 results, five-bit shift counts).
--
-- Copyright (C) 2005-2026 Mike Pall.  MIT license, see dynasm.lua.
------------------------------------------------------------------------------

local M = {}

local U32 = 0xffffffff
local TWO32 = 4294967296

local floor = math.floor

local function u32(x)
  x = floor(tonumber(x) or 0)
  x = x % TWO32
  if x < 0 then x = x + TWO32 end
  return x
end

local function s32(x)
  x = x % TWO32
  if x >= 0x80000000 then x = x - TWO32 end
  return x
end

function M.band(a, ...)
  local v = u32(a)
  for i = 1, select("#", ...) do
    v = v & u32(select(i, ...))
  end
  return s32(v)
end

function M.bor(a, ...)
  local v = u32(a)
  for i = 1, select("#", ...) do
    v = v | u32(select(i, ...))
  end
  return s32(v)
end

function M.bxor(a, ...)
  local v = u32(a)
  for i = 1, select("#", ...) do
    v = v ~ u32(select(i, ...))
  end
  return s32(v)
end

function M.bnot(a)
  return s32((~u32(a)) & U32)
end

function M.lshift(a, n)
  n = u32(n) & 31
  return s32((u32(a) << n) & U32)
end

function M.rshift(a, n)
  n = u32(n) & 31
  return s32(u32(a) >> n)
end

function M.arshift(a, n)
  n = u32(n) & 31
  return s32(floor(s32(a) / (2 ^ n)))
end

function M.rol(a, n)
  n = u32(n) & 31
  if n == 0 then return s32(u32(a)) end
  local v = u32(a)
  return s32(((v << n) | (v >> (32 - n))) & U32)
end

function M.ror(a, n)
  n = u32(n) & 31
  if n == 0 then return s32(u32(a)) end
  local v = u32(a)
  return s32(((v >> n) | (v << (32 - n))) & U32)
end

function M.bswap(a)
  local v = u32(a)
  v = ((v & 0xff00ff00) >> 8) | ((v & 0x00ff00ff) << 8)
  v = ((v & 0xffff0000) >> 16) | ((v & 0x0000ffff) << 16)
  return s32(v)
end

function M.tobit(a)
  return s32(u32(a))
end

function M.tohex(a, n)
  local v = u32(a)
  local upper = false
  n = n or 8
  if n < 0 then n = -n; upper = true end
  if n > 8 then n = 8 end
  if n < 1 then n = 8 end
  if n < 8 then v = v & ((1 << (4 * n)) - 1) end
  local fmt = upper and "%0" .. n .. "X" or "%0" .. n .. "x"
  return string.format(fmt, v)
end

return M
