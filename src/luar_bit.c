/*
** luar_bit.c -- the 'bit' library (LuaJIT 32-bit bitwise operations).
**
** Lua 5.5 has native 64-bit bitwise operators; this library exists for
** source compatibility with LuaJIT code and third-party libraries that
** call bit.band & friends.  Semantics follow the LuaJIT manual: operands
** are folded to signed 32-bit integers and shift counts use 5 bits.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luar.h"


#define BIT_MASK	0xffffffffu


/*
** LuaJIT folds numbers to 32 bits with a round-to-nearest-even conversion
** (cvtsd2si), not a C truncation: bit.tobit(1.5) is 2 and bit.tobit(3.7)
** is 4.  Match that, including the modular wrap outside the int32 range.
*/
static uint32_t num2bit (lua_Number n) {
  if (!isfinite(n))
    return 0;
  n = nearbyint(n);
  if (n >= 0.0 && n <= 4294967295.0)
    return (uint32_t)(int64_t)n;
  else if (n < 0.0 && n >= -4294967296.0)
    return (uint32_t)(int64_t)n;
  else {
    double m = fmod(n, 4294967296.0);
    if (m < 0) m += 4294967296.0;
    return (uint32_t)m;
  }
}


static uint32_t checkbit (lua_State *L, int idx) {
  lua_Number n;
#if defined(LUA_NUMBER_INTEGRAL)
  n = (lua_Number)luaL_checkinteger(L, idx);
#else
  if (lua_isinteger(L, idx))
    return (uint32_t)(lua_Integer)lua_tointeger(L, idx);
  n = (lua_Number)luaL_checknumber(L, idx);
#endif
  return num2bit(n);
}


static int bit_tobit (lua_State *L) {
  lua_pushinteger(L, (lua_Integer)(int32_t)checkbit(L, 1));
  return 1;
}


static int bit_bnot (lua_State *L) {
  lua_pushinteger(L, (lua_Integer)(int32_t)(~checkbit(L, 1)));
  return 1;
}


static int bit_band (lua_State *L) {
  int i, n = lua_gettop(L);
  uint32_t v = checkbit(L, 1);
  for (i = 2; i <= n; i++)
    v &= checkbit(L, i);
  lua_pushinteger(L, (lua_Integer)(int32_t)v);
  return 1;
}


static int bit_bor (lua_State *L) {
  int i, n = lua_gettop(L);
  uint32_t v = checkbit(L, 1);
  for (i = 2; i <= n; i++)
    v |= checkbit(L, i);
  lua_pushinteger(L, (lua_Integer)(int32_t)v);
  return 1;
}


static int bit_bxor (lua_State *L) {
  int i, n = lua_gettop(L);
  uint32_t v = checkbit(L, 1);
  for (i = 2; i <= n; i++)
    v ^= checkbit(L, i);
  lua_pushinteger(L, (lua_Integer)(int32_t)v);
  return 1;
}


static int bit_lshift (lua_State *L) {
  uint32_t x = checkbit(L, 1);
  uint32_t n = checkbit(L, 2) & 31u;
  lua_pushinteger(L, (lua_Integer)(int32_t)(x << n));
  return 1;
}


static int bit_rshift (lua_State *L) {
  uint32_t x = checkbit(L, 1);
  uint32_t n = checkbit(L, 2) & 31u;
  lua_pushinteger(L, (lua_Integer)(int32_t)(x >> n));
  return 1;
}


static int bit_arshift (lua_State *L) {
  int32_t x = (int32_t)checkbit(L, 1);
  uint32_t n = checkbit(L, 2) & 31u;
  lua_pushinteger(L, (lua_Integer)(x >> n));
  return 1;
}


static int bit_rol (lua_State *L) {
  uint32_t x = checkbit(L, 1);
  uint32_t n = checkbit(L, 2) & 31u;
  uint32_t hi = n ? (x >> (32u - n)) : 0;
  lua_pushinteger(L, (lua_Integer)(int32_t)((x << n) | hi));
  return 1;
}


static int bit_ror (lua_State *L) {
  uint32_t x = checkbit(L, 1);
  uint32_t n = checkbit(L, 2) & 31u;
  uint32_t lo = n ? (x << (32u - n)) : 0;
  lua_pushinteger(L, (lua_Integer)(int32_t)((x >> n) | lo));
  return 1;
}


static int bit_bswap (lua_State *L) {
  uint32_t x = checkbit(L, 1);
  x = ((x & 0xff00ff00u) >> 8) | ((x & 0x00ff00ffu) << 8);
  x = ((x & 0xffff0000u) >> 16) | ((x & 0x0000ffffu) << 16);
  lua_pushinteger(L, (lua_Integer)(int32_t)x);
  return 1;
}


static int bit_tohex (lua_State *L) {
  uint32_t x = checkbit(L, 1);
  int n = (int)luaL_optinteger(L, 2, 8);
  int upper = 0;
  char buf[16];
  if (n < 0) { n = -n; upper = 1; }
  if (n > 8) n = 8;
  if (n < 1) n = 8;
  if (n < 8)
    x &= (1u << (4 * n)) - 1u;   /* keep the n least significant digits */
  snprintf(buf, sizeof(buf), upper ? "%0*X" : "%0*x", n, x);
  lua_pushstring(L, buf);
  return 1;
}


static const luaL_Reg bitlib[] = {
  {"tobit", bit_tobit},
  {"bnot", bit_bnot},
  {"band", bit_band},
  {"bor", bit_bor},
  {"bxor", bit_bxor},
  {"lshift", bit_lshift},
  {"rshift", bit_rshift},
  {"arshift", bit_arshift},
  {"rol", bit_rol},
  {"ror", bit_ror},
  {"bswap", bit_bswap},
  {"tohex", bit_tohex},
  {NULL, NULL}
};


LUALIB_API int luaopen_bit (lua_State *L) {
  luaL_checkversion(L);
  luaL_newlib(L, bitlib);
  lua_pushinteger(L, 32);
  lua_setfield(L, -2, "nbits");
  return 1;
}
