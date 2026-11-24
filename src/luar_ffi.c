/*
** luar_ffi.c -- cdata values, metamethods and the ffi.* library.
**
** C types live in luar_ffi_type.c, the parser in luar_ffi_parse.c and
** libffi calls / C library namespaces in luar_ffi_call.c.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "luar.h"
#include "luar_ffi.h"
#include "luar_ffi_internal.h"


static void init_array_to (lua_State *L, FFIState *S, CType *ct,
                           void *dst, size_t len, int idx,
                           const char *what);
static void init_struct_to (lua_State *L, FFIState *S, CType *ct,
                            void *dst, int idx, const char *what);


/* ------------------------------------------------------------------ */
/* Cdata object helpers                                              */
/* ------------------------------------------------------------------ */

static int has_cdata_metatable (lua_State *L, int idx) {
  int eq;
  if (!lua_getmetatable(L, idx))
    return 0;
  luaL_getmetatable(L, LUAR_FFI_CDATA_MT);
  eq = lua_rawequal(L, -1, -2);
  lua_pop(L, 2);
  return eq;
}


CData *ffi_tocdata (lua_State *L, int idx) {
  CData *cd;
  if (lua_type(L, idx) != LUA_TUSERDATA)
    return NULL;
  /* Reject foreign userdata before touching the CData header. */
  if ((size_t)lua_rawlen(L, idx) < sizeof(CData))
    return NULL;
  cd = (CData *)lua_touserdata(L, idx);
  if (cd == NULL || cd->magic != LUAR_FFI_MAGIC)
    return NULL;
  if (!has_cdata_metatable(L, idx))
    return NULL;
  return cd;
}


int ffi_iscdata (lua_State *L, int idx) {
  return ffi_tocdata(L, idx) != NULL;
}


CData *ffi_checkcdata (lua_State *L, int idx, const char *fname) {
  CData *cd = ffi_tocdata(L, idx);
  (void)fname;
  if (cd == NULL)
    luaL_argerror(L, idx, "cdata expected");
  return cd;
}


/* LUAR: parsed type names are the hot path of ffi.new/ffi.cast/ffi.sizeof
** in numeric code.  Cache the interned ctype object keyed by the exact
** source spelling, capped so dynamically generated names cannot grow the
** registry without bound.  Only successful parses are cached; a later
** cdef can still define a typedef or complete a forward struct, because
** both cases mutate/reuse the interned CType or simply miss the cache
** until the first successful parse. */
#define LUAR_FFI_TYPECACHE	"luar.ffi.typecache"
#define LUAR_FFI_TYPECACHE_MAX	1024

static CType *ffi_parse_type_cached (lua_State *L, FFIState *S,
                                     const char *s, size_t len) {
  CType *ct;
  lua_getfield(L, LUA_REGISTRYINDEX, LUAR_FFI_TYPECACHE);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, LUAR_FFI_TYPECACHE);
  }
  lua_pushlstring(L, s, len);
  lua_rawget(L, -2);
  if (lua_type(L, -1) == LUA_TUSERDATA) {
    CData *cd = ffi_tocdata(L, -1);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) != 0) {
      ct = cd->ct;
      lua_pop(L, 2);
      return ct;
    }
  }
  lua_pop(L, 1);			/* drop the miss */
  ct = ffi_parse_type(L, S, s, len);
  {
    size_t n = lua_rawlen(L, -1);
    if (n < LUAR_FFI_TYPECACHE_MAX) {
      lua_pushboolean(L, 1);
      lua_rawseti(L, -2, (lua_Integer)n + 1);	/* count marker */
      lua_pushlstring(L, s, len);
      ffi_push_ctype(L, S, ct);
      lua_rawset(L, -3);		/* cache[name] = ctype */
    }
  }
  lua_pop(L, 1);
  return ct;
}


CType *ffi_checkctype (lua_State *L, int idx, const char *fname) {
  int t = lua_type(L, idx);
  (void)fname;
  if (t == LUA_TSTRING) {
    size_t len;
    const char *s = lua_tolstring(L, idx, &len);
    return ffi_parse_type_cached(L, ffi_getstate(L), s, len);
  }
  if (t == LUA_TUSERDATA) {
    CData *cd = ffi_tocdata(L, idx);
    if (cd != NULL)
      return cd->ct;
  }
  luaL_argerror(L, idx, "C type expected");
  return NULL;
}


CData *ffi_push_cdata_object (lua_State *L, FFIState *S, CType *ct,
                              uint32_t flags, size_t payload) {
  CData *cd;
  size_t total;
  if (payload > (size_t)-1 - CDATA_HDR_SIZE)
    luaL_error(L, "ffi: allocation size overflow");
  total = CDATA_HDR_SIZE + payload;
  cd = (CData *)lua_newuserdatauv(L, total, 2);
  memset(cd, 0, total);
  cd->ct = ct;
  cd->S = S;
  cd->flags = flags;
  cd->magic = LUAR_FFI_MAGIC;
  if ((flags & CDATA_OWNED) != 0)
    cd->v.p = (char *)cd + CDATA_HDR_SIZE;
  luaL_setmetatable(L, LUAR_FFI_CDATA_MT);
  return cd;
}


CData *ffi_push_owned (lua_State *L, FFIState *S, CType *ct,
                       size_t payload) {
  return ffi_push_cdata_object(L, S, ct, CDATA_OWNED, payload);
}


CData *ffi_push_ref (lua_State *L, FFIState *S, CType *ct, void *p,
                     int owneridx, size_t len) {
  CData *cd = ffi_push_cdata_object(L, S, ct, CDATA_REF, 0);
  cd->v.p = p;
  cd->len = len;
  if (owneridx != 0) {
    lua_pushvalue(L, owneridx);
    lua_setiuservalue(L, -2, 1);
  }
  return cd;
}


CData *ffi_push_func (lua_State *L, FFIState *S, CType *ct, void *p) {
  CData *cd = ffi_push_cdata_object(L, S, ct, CDATA_FUNC, 0);
  cd->v.p = p;
  return cd;
}


CData *ffi_push_scalar (lua_State *L, FFIState *S, CType *ct) {
  return ffi_push_cdata_object(L, S, ct, 0, 0);
}


static void *value_addr (CData *cd) {
  if ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0)
    return cd->v.p;
  return (void *)&cd->v;
}

/* C scalar-to-_Bool conversion: every non-zero scalar (including a
** fractional or infinite float) becomes 1, and a NULL pointer becomes 0.
** Truncating to an integer first would turn 0.5 into 0, which is not what
** the C compiler does for a _Bool assignment or argument. */
static int ffi_scalar_truth (lua_State *L, FFIState *S, int idx, int *ok) {
  int t = lua_type(L, idx);
  (void)S;
  if (ok != NULL) *ok = 1;
  if (t == LUA_TBOOLEAN)
    return lua_toboolean(L, idx);
  if (t == LUA_TNUMBER) {
    if (lua_isinteger(L, idx))
      return lua_tointeger(L, idx) != 0;
    return lua_tonumber(L, idx) != 0.0;   /* NaN is non-zero */
  }
  if (t == LUA_TLIGHTUSERDATA)
    return lua_touserdata(L, idx) != NULL;
  if (t == LUA_TUSERDATA) {
    CData *cd = ffi_tocdata(L, idx);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0) {
      CType *ct = cd->ct;
      void *p = value_addr(cd);
      if (ct->kind == CT_POINTER || ct->kind == CT_FUNC ||
          ct->kind == CT_ARRAY)
        return cd->v.p != NULL;
      if (ct->kind == CT_FLOAT) {
        if (ct->size == 4) {
          float f;
          memcpy(&f, p, sizeof(f));
          return f != 0.0f;
        }
        else {
          double d;
          memcpy(&d, p, sizeof(d));
          return d != 0.0;
        }
      }
      if (ffi_is_numeric(ct)) {
        if (ct->is_unsigned || ct->kind == CT_ENUM)
          return ffi_load_uint(p, ct->size) != 0;
        return ffi_load_int(p, ct->size) != 0;
      }
    }
  }
  if (ok != NULL) *ok = 0;
  return 0;
}


/* ------------------------------------------------------------------ */
/* Conversions                                                       */
/* ------------------------------------------------------------------ */

static int ctype_char_like (CType *ct) {
  if (ct == NULL)
    return 0;
  if (ct->kind == CT_VOID)
    return 1;
  return ct->kind == CT_INT && ct->size == 1;
}


static void conv_error (lua_State *L, FFIState *S, CType *ct, int idx,
                        const char *what) {
  CData *cd = ffi_tocdata(L, idx);
  const char *src;
  if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0)
    src = ffi_type_repr(S, cd->ct);
  else
    src = luaL_typename(L, idx);
  if (what != NULL && what[0] != '\0')
    luaL_error(L, "ffi: cannot convert %s to %s for %s",
               src, ffi_type_repr(S, ct), what);
  else
    luaL_error(L, "ffi: cannot convert %s to %s",
               src, ffi_type_repr(S, ct));
}


/* LUAR: a C cast from double to an integer is undefined when the value is
** not representable.  x86-64's cvttsd2si happens to produce INT64_MIN, but
** the compiler may assume the range and optimise the conversion away; GCC
** did exactly that for some constants.  LuaJIT exposes the x86 sentinel, so
** define it explicitly instead of inheriting the undefined behaviour. */
int64_t ffi_double_to_i64 (double d) {
  if (d >= -9223372036854775808.0 && d < 9223372036854775808.0)
    return (int64_t)d;
  return INT64_MIN;
}


/* LuaJIT converts the high half of the uint64 range by subtracting 2^63
** first, so [2^63, 2^64) is exact.  Above 2^64 it subtracts 2^64 and then
** uses the signed conversion, which is why 2^64 wraps to zero while 2^64 +
** 2^40 becomes 2^40 and anything that overflows after the subtraction
** keeps the 2^63 sentinel.  Negative values go through the signed
** conversion and then wrap, as C does for in-range negatives. */
uint64_t ffi_double_to_u64 (double d) {
  if (d >= 0.0) {
    if (d < 9223372036854775808.0)
      return (uint64_t)d;
    if (d < 18446744073709551616.0)
      return (uint64_t)(int64_t)(d - 9223372036854775808.0) +
             ((uint64_t)1 << 63);
    return (uint64_t)ffi_double_to_i64(d - 18446744073709551616.0);
  }
  return (uint64_t)ffi_double_to_i64(d);
}


uint64_t ffi_to_uint (lua_State *L, FFIState *S, int idx, int *ok) {
  int t = lua_type(L, idx);
  if (ok != NULL) *ok = 1;
  if (t == LUA_TBOOLEAN)
    return lua_toboolean(L, idx) ? 1u : 0u;
  if (t == LUA_TNUMBER) {
    if (lua_isinteger(L, idx))
      return (uint64_t)lua_tointeger(L, idx);
    return ffi_double_to_u64(lua_tonumber(L, idx));
  }
  if (t == LUA_TLIGHTUSERDATA)
    return (uint64_t)(uintptr_t)lua_touserdata(L, idx);
  if (t == LUA_TUSERDATA) {
    CData *cd = ffi_tocdata(L, idx);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0) {
      CType *ct = cd->ct;
      if (ct->kind == CT_POINTER || ct->kind == CT_FUNC ||
          ct->kind == CT_ARRAY)
        return (uint64_t)(uintptr_t)cd->v.p;
      if (ffi_is_numeric(ct)) {
        void *p = value_addr(cd);
        if (ct->kind == CT_FLOAT) {
          if (ct->size == 4) {
            float f;
            memcpy(&f, p, sizeof(f));
            return ffi_double_to_u64((double)f);
          }
          else {
            double d;
            memcpy(&d, p, sizeof(d));
            return ffi_double_to_u64(d);
          }
        }
        if (ct->is_unsigned || ct->kind == CT_ENUM)
          return ffi_load_uint(p, ct->size);
        return (uint64_t)ffi_load_int(p, ct->size);
      }
    }
  }
  if (ok != NULL) *ok = 0;
  (void)S;
  return 0;
}


int64_t ffi_to_int (lua_State *L, FFIState *S, int idx, int *ok) {
  int t = lua_type(L, idx);
  if (ok != NULL) *ok = 1;
  if (t == LUA_TBOOLEAN)
    return lua_toboolean(L, idx) ? 1 : 0;
  if (t == LUA_TNUMBER) {
    if (lua_isinteger(L, idx))
      return (int64_t)lua_tointeger(L, idx);
    return ffi_double_to_i64(lua_tonumber(L, idx));
  }
  if (t == LUA_TLIGHTUSERDATA)
    return (int64_t)(intptr_t)lua_touserdata(L, idx);
  if (t == LUA_TUSERDATA) {
    CData *cd = ffi_tocdata(L, idx);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0) {
      CType *ct = cd->ct;
      if (ct->kind == CT_POINTER || ct->kind == CT_FUNC ||
          ct->kind == CT_ARRAY)
        return (int64_t)(intptr_t)cd->v.p;
      if (ffi_is_numeric(ct)) {
        void *p = value_addr(cd);
        if (ct->kind == CT_FLOAT) {
          if (ct->size == 4) {
            float f;
            memcpy(&f, p, sizeof(f));
            return ffi_double_to_i64((double)f);
          }
          else {
            double d;
            memcpy(&d, p, sizeof(d));
            return ffi_double_to_i64(d);
          }
        }
        if (ct->is_unsigned || ct->kind == CT_ENUM)
          return (int64_t)ffi_load_uint(p, ct->size);
        return ffi_load_int(p, ct->size);
      }
    }
  }
  if (ok != NULL) *ok = 0;
  (void)S;
  return 0;
}


double ffi_to_double (lua_State *L, FFIState *S, int idx, int *ok) {
  int t = lua_type(L, idx);
  if (ok != NULL) *ok = 1;
  if (t == LUA_TBOOLEAN)
    return lua_toboolean(L, idx) ? 1.0 : 0.0;
  if (t == LUA_TNUMBER)
    return (double)lua_tonumber(L, idx);
  if (t == LUA_TUSERDATA) {
    CData *cd = ffi_tocdata(L, idx);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0) {
      CType *ct = cd->ct;
      if (ffi_is_numeric(ct)) {
        void *p = value_addr(cd);
        if (ct->kind == CT_FLOAT) {
          if (ct->size == 4) {
            float f;
            memcpy(&f, p, sizeof(f));
            return (double)f;
          }
          else {
            double d;
            memcpy(&d, p, sizeof(d));
            return d;
          }
        }
        if (ct->is_unsigned || ct->kind == CT_ENUM)
          return (double)ffi_load_uint(p, ct->size);
        return (double)ffi_load_int(p, ct->size);
      }
    }
  }
  if (ok != NULL) *ok = 0;
  (void)S;
  return 0;
}


void *ffi_to_ptr (lua_State *L, FFIState *S, int idx, int *ok) {
  int t = lua_type(L, idx);
  if (ok != NULL) *ok = 1;
  if (t == LUA_TNIL)
    return NULL;
  if (t == LUA_TLIGHTUSERDATA)
    return lua_touserdata(L, idx);
  if (t == LUA_TNUMBER)
    return (void *)(uintptr_t)ffi_to_uint(L, S, idx, ok);
  if (t == LUA_TUSERDATA) {
    CData *cd = ffi_tocdata(L, idx);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0) {
      CType *ct = cd->ct;
      if (ct->kind == CT_POINTER || ct->kind == CT_FUNC ||
          ct->kind == CT_ARRAY)
        return cd->v.p;
      if (ffi_is_numeric(ct)) {
        uint64_t u = ffi_to_uint(L, S, idx, ok);
        return (void *)(uintptr_t)u;
      }
    }
  }
  if (ok != NULL) *ok = 0;
  return NULL;
}


int ffi_convert (lua_State *L, FFIState *S, CType *ct, void *dst,
                 int idx, const char *what) {
  int t = lua_type(L, idx);
  switch (ct->kind) {
    case CT_BOOL: {
      int ok, truth = ffi_scalar_truth(L, S, idx, &ok);
      (void)t;
      if (!ok)
        conv_error(L, S, ct, idx, what);
      ffi_store_uint(dst, ct->size, truth != 0);
      return 1;
    }
    case CT_ENUM:
    case CT_INT: {
      uint64_t v;
      int ok;
      if (t == LUA_TNIL)
        conv_error(L, S, ct, idx, what);
      /* LUAR: signed destinations use the signed conversion so an
      ** out-of-range double gets the signed INT64_MIN sentinel, exactly as
      ** LuaJIT.  Converting through uint64 first would map 1e19 to the
      ** in-range unsigned value and then reinterpret it as a different
      ** negative integer.  Enums keep their unsigned 32-bit view. */
      if (ct->kind == CT_INT && !ct->is_unsigned)
        v = (uint64_t)ffi_to_int(L, S, idx, &ok);
      else
        v = ffi_to_uint(L, S, idx, &ok);
      if (!ok)
        conv_error(L, S, ct, idx, what);
      ffi_store_uint(dst, ct->size, v);
      return 1;
    }
    case CT_FLOAT: {
      double d;
      int ok;
      if (t == LUA_TNIL)
        conv_error(L, S, ct, idx, what);
      d = ffi_to_double(L, S, idx, &ok);
      if (!ok)
        conv_error(L, S, ct, idx, what);
      if (ct->size == 4) {
        float f = (float)d;
        memcpy(dst, &f, sizeof(f));
      }
      else {
        memcpy(dst, &d, sizeof(d));
      }
      return 1;
    }
    case CT_POINTER: {
      void *p = NULL;
      if (t == LUA_TNIL) {
        p = NULL;
      }
      else if (t == LUA_TSTRING) {
        if (!ctype_char_like(ct->child))
          conv_error(L, S, ct, idx, what);
        p = (void *)lua_tolstring(L, idx, NULL);
      }
      else {
        CData *cd = ffi_tocdata(L, idx);
        if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
            (cd->ct == ct->child || ct->child->kind == CT_VOID) &&
            (cd->ct->kind == CT_STRUCT || cd->ct->kind == CT_UNION ||
             cd->ct->kind == CT_ARRAY)) {
          /* LuaJIT accepts an aggregate lvalue where a pointer to the
          ** exact same aggregate type (or void *) is expected. */
          p = value_addr(cd);
        }
        else {
          int ok;
          p = ffi_to_ptr(L, S, idx, &ok);
          if (!ok)
            conv_error(L, S, ct, idx, what);
        }
      }
      memcpy(dst, &p, sizeof(p));
      return 1;
    }
    case CT_STRUCT:
    case CT_UNION: {
      CData *cd = ffi_tocdata(L, idx);
      if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
          cd->ct == ct) {
        if (ct->size > 0)
          memcpy(dst, value_addr(cd), ct->size);
        return 1;
      }
      if (t == LUA_TTABLE) {
        memset(dst, 0, ct->size);
        init_struct_to(L, S, ct, dst, idx, what);
        return 1;
      }
      conv_error(L, S, ct, idx, what);
      return 0;
    }
    case CT_ARRAY: {
      CData *cd = ffi_tocdata(L, idx);
      if (ct->vla)
        conv_error(L, S, ct, idx, what);
      if (t == LUA_TSTRING && ctype_char_like(ct->child)) {
        size_t n;
        const char *s = lua_tolstring(L, idx, &n);
        memset(dst, 0, ct->size);
        if (n > ct->size)
          n = ct->size;
        memcpy(dst, s, n);
        return 1;
      }
      if (t == LUA_TTABLE) {
        memset(dst, 0, ct->size);
        init_array_to(L, S, ct, dst, ct->length, idx, what);
        return 1;
      }
      if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
          cd->ct == ct) {
        if (ct->size > 0)
          memcpy(dst, value_addr(cd), ct->size);
        return 1;
      }
      conv_error(L, S, ct, idx, what);
      return 0;
    }
    case CT_FUNC: {
      CData *cd = ffi_tocdata(L, idx);
      void *p = NULL;
      if (cd == NULL || (cd->flags & CDATA_CTYPE) != 0)
        conv_error(L, S, ct, idx, what);
      if (cd->ct->kind == CT_FUNC || cd->ct->kind == CT_POINTER)
        p = cd->v.p;
      else
        conv_error(L, S, ct, idx, what);
      memcpy(dst, &p, sizeof(p));
      return 1;
    }
    case CT_VOID:
    default:
      conv_error(L, S, ct, idx, what);
      return 0;
  }
}


/* ------------------------------------------------------------------ */
/* Aggregate initialization                                          */
/* ------------------------------------------------------------------ */

static int table_has_key (lua_State *L, int idx, lua_Integer key) {
  int has;
  lua_geti(L, idx, key);
  has = !lua_isnil(L, -1);
  lua_pop(L, 1);
  return has;
}


static void init_array_to (lua_State *L, FFIState *S, CType *ct,
                           void *dst, size_t len, int idx,
                           const char *what) {
  CType *elem = ct->child;
  size_t esz = ffi_type_size(elem);
  int base;
  size_t pos = 0;
  if (esz == FFI_SIZE_UNKNOWN || esz == 0)
    luaL_error(L, "ffi: array element has unknown size");
  idx = lua_absindex(L, idx);
  base = table_has_key(L, idx, 0) ? 0 : 1;
  for (;;) {
    lua_geti(L, idx, (lua_Integer)(base + (lua_Integer)pos));
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      break;
    }
    if (pos >= len) {
      lua_pop(L, 1);
      luaL_error(L, "ffi: initializer too long for '%s'",
                 ffi_type_repr(S, ct));
    }
    ffi_convert(L, S, elem, (char *)dst + pos * esz, -1, what);
    lua_pop(L, 1);
    pos++;
  }
  if (pos == 1 && len > 1) {
    size_t i;
    for (i = 1; i < len; i++)
      memcpy((char *)dst + i * esz, dst, esz);
  }
}


/* A bitfield can start at a non-zero bit offset, so a full-width (64-bit)
** field spans nine bytes.  Keep the access in bounds by reading and writing
** through a low/high word pair instead of a single uint64_t. */
static void bitfield_read_bytes (const void *p, size_t n,
                                 uint64_t *lo, uint64_t *hi) {
  const unsigned char *bytes = (const unsigned char *)p;
  size_t i;
  *lo = 0;
  *hi = 0;
  for (i = 0; i < n; i++) {
    uint64_t b = (uint64_t)bytes[i];
    if (i < 8)
      *lo |= b << (8 * i);
    else
      *hi |= b << (8 * (i - 8));
  }
}


static void bitfield_write_bytes (void *p, size_t n,
                                  uint64_t lo, uint64_t hi) {
  unsigned char *bytes = (unsigned char *)p;
  size_t i;
  for (i = 0; i < n; i++) {
    bytes[i] = (unsigned char)((i < 8) ? (lo >> (8 * i))
                                       : (hi >> (8 * (i - 8))));
  }
}


static uint64_t bitfield_load (const CField *f, const void *p) {
  uint64_t lo, hi, raw;
  size_t n = ((size_t)f->bitoffset + f->bitwidth + 7) / 8;
  size_t off = (size_t)f->bitoffset;
  uint64_t mask = (f->bitwidth == 64) ? ~(uint64_t)0 :
                  (((uint64_t)1 << f->bitwidth) - 1);
  bitfield_read_bytes(p, n, &lo, &hi);
  if (off == 0)
    raw = lo;
  else
    raw = (lo >> off) | (hi << (64 - off));
  raw &= mask;
  if (f->type->kind == CT_BOOL)
    return (raw != 0);
  if (f->bitwidth < 64 && !f->type->is_unsigned &&
      (raw & ((uint64_t)1 << (f->bitwidth - 1))))
    raw |= ~mask;
  return raw;
}


static void bitfield_store (lua_State *L, FFIState *S, const CField *f,
                            void *p, int idx, const char *what) {
  size_t n = ((size_t)f->bitoffset + f->bitwidth + 7) / 8;
  size_t off = (size_t)f->bitoffset;
  uint64_t mask = (f->bitwidth == 64) ? ~(uint64_t)0 :
                  (((uint64_t)1 << f->bitwidth) - 1);
  uint64_t lo, hi, v;
  uint64_t mlo, mhi, vlo, vhi;
  int ok = 0;
  if (f->type->kind == CT_BOOL) {
    int truth = ffi_scalar_truth(L, S, idx, &ok);
    v = (uint64_t)(truth != 0);
  }
  else if (f->type->is_unsigned)
    v = ffi_to_uint(L, S, idx, &ok);
  else
    v = (uint64_t)ffi_to_int(L, S, idx, &ok);
  if (!ok)
    luaL_error(L, "ffi: %s needs an integer for bitfield '%s'", what,
               f->name != NULL ? f->name : "<anonymous>");
  v &= mask;
  if (off == 0) {
    mlo = mask;
    mhi = 0;
    vlo = v;
    vhi = 0;
  }
  else {
    mlo = mask << off;
    mhi = mask >> (64 - off);
    vlo = v << off;
    vhi = v >> (64 - off);
  }
  bitfield_read_bytes(p, n, &lo, &hi);
  lo = (lo & ~mlo) | vlo;
  hi = (hi & ~mhi) | vhi;
  bitfield_write_bytes(p, n, lo, hi);
}


static void field_convert (lua_State *L, FFIState *S, CField *f, void *p,
                           int idx, const char *what) {
  if (f->bitwidth != 0)
    bitfield_store(L, S, f, p, idx, what);
  else
    ffi_convert(L, S, f->type, p, idx, what);
}


/* LUAR: initialize one struct/union from a table and report whether any
** member was initialized.  A union may name any member, not just the
** first one: `ffi.new("union { int i; double d; }", {d = 1.5})` must set
** `d`, as in LuaJIT.  Positional table entries still address only the
** first member (a union has no single layout order); later members are
** selected by name.  Anonymous members are searched recursively and count
** as initialized when one of their members is. */
static int init_struct_pos (lua_State *L, FFIState *S, CType *ct,
                            void *dst, int idx, int base, int *pos,
                            int allow_pos, const char *what) {
  int did = 0;
  uint16_t i;
  idx = lua_absindex(L, idx);
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    int use_pos = allow_pos && (ct->kind != CT_UNION || i == 0);
    int field_did = 0;
    if (f->name == NULL) {
      if (f->type->kind == CT_STRUCT || f->type->kind == CT_UNION)
        field_did = init_struct_pos(L, S, f->type,
                                    (char *)dst + f->offset,
                                    idx, base, pos, use_pos, what);
      if (field_did) {
        did = 1;
        if (ct->kind == CT_UNION)
          break;
      }
      continue;
    }
    if (use_pos) {
      lua_geti(L, idx, (lua_Integer)(base + *pos));
      if (!lua_isnil(L, -1)) {
        (*pos)++;
        field_convert(L, S, f, (char *)dst + f->offset, -1, what);
        field_did = 1;
      }
      lua_pop(L, 1);
    }
    if (!field_did) {
      lua_getfield(L, idx, f->name);
      if (!lua_isnil(L, -1)) {
        field_convert(L, S, f, (char *)dst + f->offset, -1, what);
        field_did = 1;
      }
      lua_pop(L, 1);
    }
    if (field_did) {
      did = 1;
      if (ct->kind == CT_UNION)
        break;
    }
  }
  return did;
}


static void init_struct_to (lua_State *L, FFIState *S, CType *ct,
                            void *dst, int idx, const char *what) {
  int base, pos = 0;
  int has0;
  idx = lua_absindex(L, idx);
  lua_geti(L, idx, 0);
  has0 = !lua_isnil(L, -1);
  lua_pop(L, 1);
  base = has0 ? 0 : 1;
  init_struct_pos(L, S, ct, dst, idx, base, &pos, 1, what);
}


/* ------------------------------------------------------------------ */
/* ffi.new / ffi.cast                                                */
/* ------------------------------------------------------------------ */

/* LUAR: LuaJIT's FFI integer arguments are C conversions, not Lua integer
** coercions.  A finite float is truncated towards zero, while strings are
** not accepted.  Lua 5.5's luaL_checkinteger rejects 1.9 and accepts "1",
** which is the opposite of LuaJIT and surprising for a C API;
** NaN/infinity/out-of-range values are rejected instead of invoking
** undefined C conversions.  Numeric cdata is accepted with the same rules,
** so a length or index can be an int64/uint64/enum value. */
static lua_Integer check_c_int_arg (lua_State *L, int idx, const char *what,
                                    int optional, lua_Integer def) {
  if (lua_isnoneornil(L, idx)) {
    if (optional)
      return def;
    luaL_error(L, "ffi: %s is required", what);
  }
  if (lua_type(L, idx) == LUA_TNUMBER) {
    if (lua_isinteger(L, idx))
      return lua_tointeger(L, idx);
    {
      lua_Number n = lua_tonumber(L, idx);
      if (!(n >= -9223372036854775808.0 && n < 9223372036854775808.0))
        luaL_error(L, "ffi: %s is out of range", what);
      return (lua_Integer)n;
    }
  }
  {
    CData *cd = ffi_tocdata(L, idx);
    if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
        ffi_is_numeric(cd->ct)) {
      FFIState *S = ffi_getstate(L);
      if (cd->ct->kind == CT_FLOAT) {
        int ok = 1;
        lua_Number n = ffi_to_double(L, S, idx, &ok);
        if (!ok || !(n >= -9223372036854775808.0 &&
                     n < 9223372036854775808.0))
          luaL_error(L, "ffi: %s is out of range", what);
        return (lua_Integer)n;
      }
      {
        int ok = 1;
        lua_Integer n = ffi_to_int(L, S, idx, &ok);
        if (!ok)
          luaL_error(L, "ffi: %s must be a number", what);
        return n;
      }
    }
  }
  luaL_error(L, "ffi: %s must be a number", what);
  return 0;
}


static size_t check_len_arg (lua_State *L, int idx, const char *what) {
  lua_Integer n = check_c_int_arg(L, idx, what, 0, 0);
  if (n < 0)
    luaL_error(L, "ffi: negative %s", what);
  if ((uint64_t)n > (uint64_t)FFI_MAX_NEW)
    luaL_error(L, "ffi: %s is too large", what);
  return (size_t)n;
}


static void init_array_args (lua_State *L, FFIState *S, CType *ct,
                            void *dst, size_t len, int firstarg, int top,
                            const char *what) {
  CType *elem = ct->child;
  size_t esz = ffi_type_size(elem);
  size_t pos = 0;
  int i;
  if (esz == FFI_SIZE_UNKNOWN || esz == 0)
    luaL_error(L, "ffi: array element has unknown size");
  for (i = firstarg; i <= top; i++) {
    if (pos >= len)
      luaL_error(L, "ffi: too many initializers for '%s'",
                 ffi_type_repr(S, ct));
    ffi_convert(L, S, elem, (char *)dst + pos * esz, i, what);
    pos++;
  }
  if (pos == 1 && len > 1) {
    size_t j;
    for (j = 1; j < len; j++)
      memcpy((char *)dst + j * esz, dst, esz);
  }
}


static void init_struct_args (lua_State *L, FFIState *S, CType *ct,
                              void *dst, int *argi, int top,
                              const char *what) {
  uint16_t i;
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    if (f->name == NULL) {
      if (f->type->kind == CT_STRUCT || f->type->kind == CT_UNION) {
        init_struct_args(L, S, f->type, (char *)dst + f->offset,
                         argi, top, what);
        if (ct->kind == CT_UNION)
          break;
      }
      continue;
    }
    if (*argi > top)
      break;
    field_convert(L, S, f, (char *)dst + f->offset, *argi, what);
    (*argi)++;
    if (ct->kind == CT_UNION)
      break;
  }
}


static int pointer_base_compatible (CType *a, CType *b);


/* LUAR: ffi.new is not an ffi.cast.  LuaJIT accepts only nil, a light
** userdata, a compatible pointer/array (with void * matching anything), or
** an aggregate of the exact pointee type; numbers, numeric cdata and
** incompatible pointers are rejected.  ffi.cast and assignments keep the
** permissive C conversions. */
static int ffi_new_pointer (lua_State *L, FFIState *S, CType *ct, int idx,
                            void *dst) {
  int t = lua_type(L, idx);
  void *p = NULL;
  if (t == LUA_TNIL) {
    p = NULL;
  }
  else if (t == LUA_TSTRING) {
    /* Luar accepts a Lua string for character pointer destinations (the
    ** existing convenience used by ffi.new("char *", "abc")).  A void *
    ** is not a character type: LuaJIT rejects a string initializer there,
    ** and silently reinterpreting the bytes hides a real type error.
    ** ffi.cast and string arguments in calls stay permissive. */
    if (ct->child->kind == CT_INT && ct->child->size == 1)
      p = (void *)lua_tolstring(L, idx, NULL);
    else
      conv_error(L, S, ct, idx, NULL);
  }
  else if (t == LUA_TLIGHTUSERDATA) {
    p = lua_touserdata(L, idx);
  }
  else {
    CData *src = ffi_tocdata(L, idx);
    if (src == NULL || (src->flags & CDATA_CTYPE) != 0)
      conv_error(L, S, ct, idx, NULL);
    if ((src->ct == ct->child || ct->child->kind == CT_VOID) &&
        (src->ct->kind == CT_STRUCT || src->ct->kind == CT_UNION ||
         src->ct->kind == CT_ARRAY)) {
      /* Any aggregate lvalue converts to an object pointer, and to void *
      ** in particular, as in LuaJIT. */
      p = value_addr(src);
    }
    else if (src->ct->kind == CT_POINTER || src->ct->kind == CT_ARRAY) {
      CType *sbase = src->ct->child;
      if (ct->child->kind == CT_VOID || sbase->kind == CT_VOID ||
          pointer_base_compatible(ct->child, sbase))
        p = (src->ct->kind == CT_ARRAY) ? value_addr(src) : src->v.p;
      else
        conv_error(L, S, ct, idx, NULL);
    }
    else if (src->ct->kind == CT_FUNC) {
      if (ct->child->kind == CT_VOID || ct->child->kind == CT_FUNC)
        p = src->v.p;
      else
        conv_error(L, S, ct, idx, NULL);
    }
    else {
      conv_error(L, S, ct, idx, NULL);
    }
  }
  memcpy(dst, &p, sizeof(p));
  return 1;
}


int ffi_do_new (lua_State *L, FFIState *S, CType *ct, int argstart) {
  int argi = argstart;
  int total = lua_gettop(L);
  if (ct->kind == CT_VOID)
    luaL_error(L, "ffi: cannot instantiate 'void'");
  if (ct->kind == CT_FUNC)
    luaL_error(L, "ffi: cannot instantiate a function type");
  if (ct->kind == CT_ARRAY) {
    CType *elem = ct->child;
    size_t esz = ffi_type_size(elem);
    size_t n;
    size_t bytes;
    CData *cd;
    if (esz == FFI_SIZE_UNKNOWN || esz == 0)
      luaL_error(L, "ffi: array element has unknown size");
    if (ct->vla) {
      if (lua_isnoneornil(L, argi))
        luaL_error(L, "ffi: length required for variable-length array");
      n = check_len_arg(L, argi, "array length");
      argi++;
    }
    else {
      n = ct->length;
    }
    if (n > FFI_MAX_NEW / esz)
      luaL_error(L, "ffi: array allocation is too large");
    bytes = n * esz;
    cd = ffi_push_owned(L, S, ct, bytes);
    cd->len = n;
    if (argi <= total) {
      int nrem = total - argi + 1;
      int t = lua_type(L, argi);
      if (nrem == 1 && t == LUA_TSTRING) {
        size_t slen;
        const char *str;
        if (!ctype_char_like(elem))
          luaL_error(L, "ffi: cannot initialize '%s' with a string",
                     ffi_type_repr(S, ct));
        str = lua_tolstring(L, argi, &slen);
        if (slen > bytes)
          slen = bytes;
        memcpy(cd->v.p, str, slen);
      }
      else if (nrem == 1 && t == LUA_TTABLE) {
        init_array_to(L, S, ct, cd->v.p, n, argi, "array initializer");
      }
      else if (nrem == 1 && t == LUA_TUSERDATA) {
        CData *src = ffi_tocdata(L, argi);
        if (src != NULL && (src->flags & CDATA_CTYPE) == 0 &&
            src->ct == ct) {
          if (ffi_cdata_size(src) != bytes)
            luaL_error(L, "ffi: incompatible array initializer for '%s'",
                       ffi_type_repr(S, ct));
          memcpy(cd->v.p, value_addr(src), bytes);
        }
        else if (src != NULL && (src->flags & CDATA_CTYPE) == 0 &&
                 (src->ct->kind == CT_ARRAY ||
                  src->ct->kind == CT_STRUCT ||
                  src->ct->kind == CT_UNION ||
                  src->ct->kind == CT_POINTER ||
                  src->ct->kind == CT_FUNC)) {
          /* LUAR: one aggregate/pointer cdata is an element initializer,
          ** not an array to copy.  The old fall-through converted its
          ** address to the element type and then broadcast that scalar,
          ** which both read an array out of bounds and silently produced
          ** garbage for a mismatched source array. */
          luaL_error(L, "ffi: incompatible array initializer for '%s'",
                     ffi_type_repr(S, ct));
        }
        else {
          init_array_args(L, S, ct, cd->v.p, n, argi, total,
                          "array initializer");
        }
      }
      else {
        init_array_args(L, S, ct, cd->v.p, n, argi, total,
                        "array initializer");
      }
    }
    return 1;
  }
  if (ct->kind == CT_STRUCT || ct->kind == CT_UNION) {
    CData *cd;
    if (ct->incomplete)
      luaL_error(L, "ffi: '%s' is an incomplete type",
                 ffi_type_repr(S, ct));
    cd = ffi_push_owned(L, S, ct, ct->size);
    if (argi <= total) {
      int nrem = total - argi + 1;
      int t = lua_type(L, argi);
      if (nrem == 1 && t == LUA_TTABLE) {
        init_struct_to(L, S, ct, cd->v.p, argi, "initializer");
      }
      else if (nrem == 1 && t == LUA_TUSERDATA) {
        CData *src = ffi_tocdata(L, argi);
        if (src != NULL && src->ct == ct) {
          memcpy(cd->v.p, value_addr(src), ct->size);
        }
        else {
          int ai = argi;
          init_struct_args(L, S, ct, cd->v.p, &ai, total, "initializer");
          if (ai <= total)
            luaL_error(L, "ffi: too many initializers for '%s'",
                       ffi_type_repr(S, ct));
        }
      }
      else {
        int ai = argi;
        init_struct_args(L, S, ct, cd->v.p, &ai, total, "initializer");
        if (ai <= total)
          luaL_error(L, "ffi: too many initializers for '%s'",
                     ffi_type_repr(S, ct));
      }
    }
    return 1;
  }
  else {
    CData *cd;
    if (ct->size > 16)
      luaL_error(L, "ffi: scalar type '%s' is too large",
                 ffi_type_repr(S, ct));
    cd = ffi_push_scalar(L, S, ct);
    if (argi <= total) {
      if (argi < total)
        luaL_error(L, "ffi: too many initializers for '%s'",
                   ffi_type_repr(S, ct));
      if (ct->kind == CT_POINTER)
        ffi_new_pointer(L, S, ct, argi, &cd->v);
      else
        ffi_convert(L, S, ct, &cd->v, argi, NULL);
    }
    return 1;
  }
}


static int ffi_new (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CType *ct = ffi_checkctype(L, 1, "new");
  return ffi_do_new(L, S, ct, 2);
}


static int ffi_cast (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CType *ct = ffi_checkctype(L, 1, "cast");
  luaL_checkany(L, 2);
  if (ct->kind == CT_VOID)
    luaL_error(L, "ffi: cannot cast to 'void'");
  if (ct->kind == CT_FUNC)
    luaL_error(L, "ffi: cannot cast to a function type; "
                  "use a function pointer type");
  if (ct->kind == CT_POINTER) {
    CData *cd;
    void *p;
    if (lua_isnil(L, 2)) {
      p = NULL;
    }
    else if (lua_type(L, 2) == LUA_TSTRING) {
      /* LUAR: LuaJIT's ffi.cast accepts a Lua string for any pointer type,
      ** not just char pointers.  The string is not anchored by the cdata;
      ** callers must keep it alive, as documented. */
      p = (void *)lua_tolstring(L, 2, NULL);
    }
    else {
      CData *src = ffi_tocdata(L, 2);
      if (src != NULL && (src->flags & CDATA_CTYPE) == 0 &&
          (src->ct->kind == CT_STRUCT || src->ct->kind == CT_UNION ||
           src->ct->kind == CT_ARRAY)) {
        /* CCF_CAST allows taking the address of any aggregate cdata. */
        p = value_addr(src);
      }
      else {
        int ok;
        p = ffi_to_ptr(L, S, 2, &ok);
        if (!ok)
          conv_error(L, S, ct, 2, "cast");
      }
    }
    cd = ffi_push_scalar(L, S, ct);
    cd->v.p = p;
    return 1;
  }
  if (ct->kind == CT_STRUCT || ct->kind == CT_UNION ||
      ct->kind == CT_ARRAY) {
    CData *src = ffi_tocdata(L, 2);
    CData *cd;
    if (src != NULL && src->ct == ct) {
      if (ct->kind == CT_ARRAY && ct->vla)
        luaL_error(L, "ffi: cannot cast to a variable-length array");
      cd = ffi_push_owned(L, S, ct, ct->size);
      memcpy(cd->v.p, value_addr(src), ct->size);
      return 1;
    }
    if (ct->kind == CT_ARRAY && ct->vla)
      luaL_error(L, "ffi: cannot cast to a variable-length array");
    conv_error(L, S, ct, 2, "cast");
  }
  {
    CData *cd = ffi_push_scalar(L, S, ct);
    ffi_convert(L, S, ct, &cd->v, 2, "cast");
    return 1;
  }
}


/* ------------------------------------------------------------------ */
/* Field / array access                                              */
/* ------------------------------------------------------------------ */

static CType *meta_ctype (CType *ct) {
  if (ct->kind == CT_STRUCT || ct->kind == CT_UNION)
    return ct;
  if (ct->kind == CT_POINTER && ct->child != NULL &&
      (ct->child->kind == CT_STRUCT || ct->child->kind == CT_UNION))
    return ct->child;
  return NULL;
}


static int meta_index (lua_State *L, CType *ct, int keyidx) {
  if (ct->metatype_ref == LUA_NOREF)
    return 0;
  lua_rawgeti(L, LUA_REGISTRYINDEX, ct->metatype_ref);
  lua_getfield(L, -1, "__index");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 2);
    return 0;
  }
  if (lua_istable(L, -1)) {
    lua_pushvalue(L, keyidx);
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_remove(L, -2);
    return 1;
  }
  if (lua_isfunction(L, -1)) {
    lua_pushvalue(L, 1);
    lua_pushvalue(L, keyidx);
    lua_call(L, 2, 1);
    lua_remove(L, -2);
    return 1;
  }
  lua_pop(L, 2);
  luaL_error(L, "ffi: metatype __index must be a table or function");
  return 0;
}


static int meta_newindex (lua_State *L, CType *ct) {
  if (ct->metatype_ref == LUA_NOREF)
    return 0;
  lua_rawgeti(L, LUA_REGISTRYINDEX, ct->metatype_ref);
  lua_getfield(L, -1, "__newindex");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 2);
    return 0;
  }
  if (lua_istable(L, -1)) {
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);
    lua_pop(L, 2);
    return 1;
  }
  if (lua_isfunction(L, -1)) {
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_call(L, 3, 0);
    lua_pop(L, 1);
    return 1;
  }
  lua_pop(L, 2);
  luaL_error(L, "ffi: metatype __newindex must be a table or function");
  return 0;
}


/* Resolve a struct/union field access.  Returns 1 when handled. */
static int cdata_field (lua_State *L, FFIState *S, CData *cd,
                        const char *name) {
  CType *ct = cd->ct;
  CType *st;
  void *base;
  size_t off = 0;
  CType *ft = NULL;
  CField *f;
  if (ct->kind == CT_STRUCT || ct->kind == CT_UNION) {
    st = ct;
    base = value_addr(cd);
  }
  else if (ct->kind == CT_POINTER &&
           (ct->child->kind == CT_STRUCT || ct->child->kind == CT_UNION)) {
    st = ct->child;
    if (cd->v.p == NULL)
      luaL_error(L, "ffi: attempt to access a field of a NULL pointer");
    base = cd->v.p;
  }
  else {
    return 0;
  }
  f = ffi_type_find_field(st, name, &off, &ft);
  if (f == NULL)
    return 0;
  {
    void *p = (char *)base + off;
    if (f->bitwidth != 0) {
      /* LUAR: a bitfield has the same Lua-visible type as an ordinary
      ** field of its declared base type.  _Bool yields a boolean; an
      ** 8-byte integer stays cdata (so an unsigned 64-bit all-ones field
      ** is 18446744073709551615, not the Lua integer -1); narrower
      ** integers and enums are Lua numbers. */
      uint64_t raw = bitfield_load(f, p);
      if (ft->kind == CT_BOOL)
        lua_pushboolean(L, raw != 0);
      else if (ft->kind == CT_INT && ft->size == 8) {
        CData *bcd = ffi_push_scalar(L, S, ft);
        ffi_store_uint(&bcd->v, 8, raw);
      }
      else
        lua_pushinteger(L, (lua_Integer)raw);
      return 1;
    }
    if (ft->kind == CT_STRUCT || ft->kind == CT_UNION ||
        ft->kind == CT_ARRAY) {
      size_t len = (ft->kind == CT_ARRAY && !ft->vla) ? ft->length : 0;
      ffi_push_ref(L, S, ft, p, 1, len);
    }
    else {
      ffi_push_value(L, S, ft, p, 1);
    }
  }
  return 1;
}


static int cdata_field_set (lua_State *L, FFIState *S, CData *cd,
                            const char *name) {
  CType *ct = cd->ct;
  CType *st;
  void *base;
  size_t off = 0;
  CType *ft = NULL;
  CField *f;
  if (ct->kind == CT_STRUCT || ct->kind == CT_UNION) {
    st = ct;
    base = value_addr(cd);
  }
  else if (ct->kind == CT_POINTER &&
           (ct->child->kind == CT_STRUCT || ct->child->kind == CT_UNION)) {
    st = ct->child;
    if (cd->v.p == NULL)
      luaL_error(L, "ffi: attempt to access a field of a NULL pointer");
    base = cd->v.p;
  }
  else {
    return 0;
  }
  f = ffi_type_find_field(st, name, &off, &ft);
  if (f == NULL)
    return 0;
  if (f->bitwidth != 0)
    bitfield_store(L, S, f, (char *)base + off, 3, "field assignment");
  else
    ffi_convert(L, S, ft, (char *)base + off, 3, "field assignment");
  return 1;
}


static int cdata_get_index (lua_State *L, FFIState *S, CData *cd,
                            lua_Integer idx) {
  CType *ct = cd->ct;
  CType *elem;
  void *base;
  size_t esz;
  size_t len = 0;
  int bounded = 0;
  char *p;
  if (ct->kind == CT_ARRAY) {
    elem = ct->child;
    base = value_addr(cd);
    len = ct->vla ? cd->len : ct->length;
    bounded = 1;
  }
  else if (ct->kind == CT_POINTER) {
    elem = ct->child;
    base = cd->v.p;
    if (base == NULL)
      luaL_error(L, "ffi: attempt to index a NULL pointer");
  }
  else {
    luaL_error(L, "ffi: '%s' cannot be indexed",
               ffi_type_repr(S, ct));
    return 0;
  }
  if (elem->kind == CT_FUNC)
    luaL_error(L, "ffi: cannot index a function pointer");
  esz = ffi_type_size(elem);
  if (esz == FFI_SIZE_UNKNOWN || esz == 0)
    luaL_error(L, "ffi: element type has unknown size");
  if (bounded && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    luaL_error(L, "ffi: array index %I out of bounds (size %I)",
               (lua_Integer)idx, (lua_Integer)len);
  p = (char *)base + (ptrdiff_t)idx * (ptrdiff_t)esz;
  if (elem->kind == CT_STRUCT || elem->kind == CT_UNION ||
      elem->kind == CT_ARRAY) {
    size_t elen = (elem->kind == CT_ARRAY && !elem->vla) ? elem->length : 0;
    ffi_push_ref(L, S, elem, p, 1, elen);
  }
  else {
    ffi_push_value(L, S, elem, p, 1);
  }
  return 1;
}


static int cdata_set_index (lua_State *L, FFIState *S, CData *cd,
                            lua_Integer idx) {
  CType *ct = cd->ct;
  CType *elem;
  void *base;
  size_t esz;
  size_t len = 0;
  int bounded = 0;
  char *p;
  if (ct->kind == CT_ARRAY) {
    elem = ct->child;
    base = value_addr(cd);
    len = ct->vla ? cd->len : ct->length;
    bounded = 1;
  }
  else if (ct->kind == CT_POINTER) {
    elem = ct->child;
    base = cd->v.p;
    if (base == NULL)
      luaL_error(L, "ffi: attempt to index a NULL pointer");
  }
  else {
    luaL_error(L, "ffi: '%s' cannot be indexed",
               ffi_type_repr(S, ct));
    return 0;
  }
  if (elem->kind == CT_FUNC)
    luaL_error(L, "ffi: cannot index a function pointer");
  esz = ffi_type_size(elem);
  if (esz == FFI_SIZE_UNKNOWN || esz == 0)
    luaL_error(L, "ffi: element type has unknown size");
  if (bounded && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    luaL_error(L, "ffi: array index %I out of bounds (size %I)",
               (lua_Integer)idx, (lua_Integer)len);
  p = (char *)base + (ptrdiff_t)idx * (ptrdiff_t)esz;
  ffi_convert(L, S, elem, p, 3, "array assignment");
  return 1;
}


/* LuaJIT accepts an integer cdata as an array index, but rejects float and
** bool cdata (even though the latter are numeric for length arguments). */
static int cdata_integer_index (lua_State *L, int idx) {
  CData *cd = ffi_tocdata(L, idx);
  return cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
         (cd->ct->kind == CT_INT || cd->ct->kind == CT_ENUM);
}


static int cdata_index (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *cd = ffi_checkcdata(L, 1, "__index");
  CType *ct = cd->ct;
  if ((cd->flags & CDATA_CTYPE) != 0)
    luaL_error(L, "ffi: cannot index a C type");
  if (lua_type(L, 2) == LUA_TSTRING) {
    size_t n;
    const char *name = lua_tolstring(L, 2, &n);
    (void)n;
    if ((cd->flags & CDATA_CALLBACK) != 0) {
      if (strcmp(name, "free") == 0) {
        lua_pushcfunction(L, ffi_callback_free_mt);
        return 1;
      }
      if (strcmp(name, "set") == 0) {
        lua_pushcfunction(L, ffi_callback_set_mt);
        return 1;
      }
    }
    if (cdata_field(L, S, cd, name))
      return 1;
    {
      CType *mct = meta_ctype(ct);
      if (mct != NULL && meta_index(L, mct, 2))
        return 1;
    }
    luaL_error(L, "ffi: '%s' has no member named '%s'",
               ffi_type_repr(S, ct), name);
  }
  if (lua_type(L, 2) == LUA_TUSERDATA && cdata_integer_index(L, 2)) {
    if (ct->kind == CT_ARRAY || ct->kind == CT_POINTER)
      return cdata_get_index(L, S, cd,
                             check_c_int_arg(L, 2, "array index", 0, 0));
  }
  if (lua_type(L, 2) == LUA_TNUMBER) {
    if (ct->kind == CT_ARRAY || ct->kind == CT_POINTER)
      return cdata_get_index(L, S, cd,
                             check_c_int_arg(L, 2, "array index", 0, 0));
    {
      CType *mct = meta_ctype(ct);
      if (mct != NULL && meta_index(L, mct, 2))
        return 1;
    }
    luaL_error(L, "ffi: '%s' cannot be indexed",
               ffi_type_repr(S, ct));
  }
  {
    CType *mct = meta_ctype(ct);
    if (mct != NULL && meta_index(L, mct, 2))
      return 1;
  }
  luaL_error(L, "ffi: bad index for '%s'", ffi_type_repr(S, ct));
  return 0;
}


static int cdata_newindex (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *cd = ffi_checkcdata(L, 1, "__newindex");
  CType *ct = cd->ct;
  if ((cd->flags & CDATA_CTYPE) != 0)
    luaL_error(L, "ffi: cannot index a C type");
  luaL_checkany(L, 3);
  if (lua_type(L, 2) == LUA_TSTRING) {
    size_t n;
    const char *name = lua_tolstring(L, 2, &n);
    (void)n;
    if (cdata_field_set(L, S, cd, name))
      return 0;
    {
      CType *mct = meta_ctype(ct);
      if (mct != NULL && meta_newindex(L, mct))
        return 0;
    }
    luaL_error(L, "ffi: '%s' has no member named '%s'",
               ffi_type_repr(S, ct), name);
  }
  if (lua_type(L, 2) == LUA_TUSERDATA && cdata_integer_index(L, 2)) {
    if (ct->kind == CT_ARRAY || ct->kind == CT_POINTER) {
      cdata_set_index(L, S, cd,
                      check_c_int_arg(L, 2, "array index", 0, 0));
      return 0;
    }
  }
  if (lua_type(L, 2) == LUA_TNUMBER) {
    if (ct->kind == CT_ARRAY || ct->kind == CT_POINTER) {
      cdata_set_index(L, S, cd,
                      check_c_int_arg(L, 2, "array index", 0, 0));
      return 0;
    }
    {
      CType *mct = meta_ctype(ct);
      if (mct != NULL && meta_newindex(L, mct))
        return 0;
    }
    luaL_error(L, "ffi: '%s' cannot be indexed",
               ffi_type_repr(S, ct));
  }
  {
    CType *mct = meta_ctype(ct);
    if (mct != NULL && meta_newindex(L, mct))
      return 0;
  }
  luaL_error(L, "ffi: bad index for '%s'", ffi_type_repr(S, ct));
  return 0;
}


/* ------------------------------------------------------------------ */
/* Value conversion to Lua                                          */
/* ------------------------------------------------------------------ */

void ffi_push_value (lua_State *L, FFIState *S, CType *ct, void *p,
                     int owneridx) {
  ffi_push_value_len(L, S, ct, p, owneridx,
                     (ct->kind == CT_ARRAY && !ct->vla) ? ct->length : 0);
}


void ffi_push_value_len (lua_State *L, FFIState *S, CType *ct, void *p,
                         int owneridx, size_t len) {
  switch (ct->kind) {
    case CT_BOOL:
      lua_pushboolean(L, ffi_load_uint(p, ct->size) != 0);
      break;
    case CT_ENUM: {
      /* LUAR: enum values stored as -1 display as -1 but convert as the
      ** unsigned 32-bit value 4294967295, matching LuaJIT's numeric view. */
      lua_pushinteger(L, (lua_Integer)ffi_load_uint(p, ct->size));
      break;
    }
    case CT_INT:
      if (ct->size == 8) {
        CData *cd = ffi_push_scalar(L, S, ct);
        if (ct->size > 0)
          memcpy(&cd->v, p, ct->size);
      }
      else if (ct->is_unsigned)
        lua_pushinteger(L, (lua_Integer)ffi_load_uint(p, ct->size));
      else
        lua_pushinteger(L, (lua_Integer)ffi_load_int(p, ct->size));
      break;
    case CT_FLOAT:
      if (ct->size == 4) {
        float f;
        memcpy(&f, p, sizeof(f));
        lua_pushnumber(L, (lua_Number)f);
      }
      else {
        double d;
        memcpy(&d, p, sizeof(d));
        lua_pushnumber(L, (lua_Number)d);
      }
      break;
    case CT_POINTER: {
      CData *cd = ffi_push_scalar(L, S, ct);
      void *v;
      memcpy(&v, p, sizeof(v));
      cd->v.p = v;
      break;
    }
    case CT_FUNC: {
      void *v;
      memcpy(&v, p, sizeof(v));
      ffi_push_func(L, S, ct, v);
      break;
    }
    case CT_STRUCT:
    case CT_UNION:
    case CT_ARRAY:
      ffi_push_ref(L, S, ct, p, owneridx, len);
      break;
    default:
      luaL_error(L, "ffi: cannot convert '%s' to a Lua value",
                 ffi_type_repr(S, ct));
  }
}


/* ------------------------------------------------------------------ */
/* Metamethods                                                      */
/* ------------------------------------------------------------------ */

static int meta_tostring (lua_State *L, CType *ct, int idx) {
  if (ct->metatype_ref == LUA_NOREF)
    return 0;
  lua_rawgeti(L, LUA_REGISTRYINDEX, ct->metatype_ref);
  lua_getfield(L, -1, "__tostring");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 2);
    return 0;
  }
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    luaL_error(L, "ffi: metatype __tostring must be a function");
  }
  lua_pushvalue(L, idx);
  lua_call(L, 1, 1);
  lua_remove(L, -2);
  return 1;
}


/* Defined with the comparison metamethods below. */
static CType *cdata_metatype (CData *cd);
static int meta_push (lua_State *L, CType *ct, const char *name);
static int meta_binop (lua_State *L, const char *name);


static int cdata_len (lua_State *L) {
  CData *cd = ffi_tocdata(L, 1);
  CType *mct = (cd != NULL && (cd->flags & CDATA_CTYPE) == 0)
                   ? meta_ctype(cd->ct) : NULL;
  if (meta_push(L, mct, "__len")) {
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    return 1;
  }
  luaL_error(L, "attempt to get length of a cdata value");
  return 0;
}


/* LuaJIT applies a metatype's __pairs to a struct/union value and to a
** pointer to that type.  Lua 5.5 has no __ipairs, so only pairs() is
** dispatched here. */
static int cdata_pairs (lua_State *L) {
  CData *cd = ffi_tocdata(L, 1);
  CType *mct = (cd != NULL && (cd->flags & CDATA_CTYPE) == 0)
                   ? meta_ctype(cd->ct) : NULL;
  if (meta_push(L, mct, "__pairs")) {
    lua_insert(L, 1);          /* function, cdata */
    lua_call(L, 1, LUA_MULTRET);
    return lua_gettop(L);
  }
  luaL_error(L, "attempt to iterate a cdata value");
  return 0;
}


static int cdata_concat (lua_State *L) {
  if (meta_binop(L, "__concat")) return 1;
  luaL_error(L, "attempt to concatenate a cdata value");
  return 0;
}


static int cdata_tostring (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *cd = ffi_checkcdata(L, 1, "__tostring");
  CType *ct = cd->ct;
  char buf[512];
  if ((cd->flags & CDATA_CTYPE) != 0) {
    snprintf(buf, sizeof(buf), "ctype<%s>", ffi_type_repr(S, ct));
    lua_pushstring(L, buf);
    return 1;
  }
  {
    CType *mct = meta_ctype(ct);
    if (mct != NULL && meta_tostring(L, mct, 1))
      return 1;
  }
  if (ct->kind == CT_INT && ct->size == 8) {
    if (ct->is_unsigned) {
      uint64_t u = ffi_load_uint(&cd->v, 8);
      snprintf(buf, sizeof(buf), "%lluULL", (unsigned long long)u);
    }
    else {
      int64_t v = ffi_load_int(&cd->v, 8);
      snprintf(buf, sizeof(buf), "%lldLL", (long long)v);
    }
    lua_pushstring(L, buf);
    return 1;
  }
  if (ct->kind == CT_ENUM) {
    int32_t v = (int32_t)ffi_load_int(value_addr(cd), ct->size);
    snprintf(buf, sizeof(buf), "cdata<%s>: %d",
             ffi_type_repr(S, ct), (int)v);
    lua_pushstring(L, buf);
    return 1;
  }
  if (ct->kind == CT_POINTER || ct->kind == CT_FUNC) {
    void *p = cd->v.p;
    if (p == NULL)
      snprintf(buf, sizeof(buf), "cdata<%s>: NULL",
               ffi_type_repr(S, ct));
    else
      snprintf(buf, sizeof(buf), "cdata<%s>: %p",
               ffi_type_repr(S, ct), p);
    lua_pushstring(L, buf);
    return 1;
  }
  if (ct->kind == CT_STRUCT || ct->kind == CT_UNION ||
      ct->kind == CT_ARRAY) {
    snprintf(buf, sizeof(buf), "cdata<%s>: %p",
             ffi_type_repr(S, ct), value_addr(cd));
    lua_pushstring(L, buf);
    return 1;
  }
  snprintf(buf, sizeof(buf), "cdata<%s>: %p",
           ffi_type_repr(S, ct), (void *)&cd->v);
  lua_pushstring(L, buf);
  return 1;
}


static int cdata_is_unsigned64 (CData *cd) {
  return cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
         ffi_is_numeric(cd->ct) && cd->ct->kind != CT_FLOAT &&
         cd->ct->is_unsigned && cd->ct->size == 8;
}


static int numeric_operand (lua_State *L, int idx) {
  CData *cd = ffi_tocdata(L, idx);
  if (cd != NULL)
    return (cd->flags & CDATA_CTYPE) == 0 && ffi_is_numeric(cd->ct);
  return lua_type(L, idx) == LUA_TNUMBER;
}


/* LuaJIT promotes all cdata numeric comparisons to 64 bit integers,
** using uint64_t if either operand is an unsigned 64 bit value. */
static int numeric_compare (lua_State *L, FFIState *S, int i1, int i2,
                            int *cmp) {
  CData *a = ffi_tocdata(L, i1);
  CData *b = ffi_tocdata(L, i2);
  int uns = cdata_is_unsigned64(a) || cdata_is_unsigned64(b);
  if (!numeric_operand(L, i1) || !numeric_operand(L, i2))
    return 0;
  if (uns) {
    int ok1, ok2;
    uint64_t x = ffi_to_uint(L, S, i1, &ok1);
    uint64_t y = ffi_to_uint(L, S, i2, &ok2);
    if (!ok1 || !ok2)
      return 0;
    *cmp = (x < y) ? -1 : (x > y) ? 1 : 0;
  }
  else {
    int ok1, ok2;
    int64_t x = ffi_to_int(L, S, i1, &ok1);
    int64_t y = ffi_to_int(L, S, i2, &ok2);
    if (!ok1 || !ok2)
      return 0;
    *cmp = (x < y) ? -1 : (x > y) ? 1 : 0;
  }
  return 1;
}


/* LuaJIT dispatches a metatype's __eq/__lt/__le/__len for struct and
** union cdata.  Other cdata (numbers, pointers, arrays, CType objects)
** keep the built-in FFI semantics. */
static CType *cdata_metatype (CData *cd) {
  if (cd == NULL || (cd->flags & CDATA_CTYPE) != 0 || cd->ct == NULL)
    return NULL;
  if (cd->ct->kind != CT_STRUCT && cd->ct->kind != CT_UNION)
    return NULL;
  if (cd->ct->metatype_ref == LUA_NOREF)
    return NULL;
  return cd->ct;
}


/* LuaJIT treats an array cdata as a pointer to its first element for
** arithmetic, and compares its address with other pointer-like cdata.
** Arrays cannot have an element type that is a function (the parser
** rejects int(*)[N] shapes at the array level), so only POINTER, ARRAY and
** FUNC are address-like here. */
static int cdata_is_array (CData *cd) {
  return cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
         cd->ct != NULL && cd->ct->kind == CT_ARRAY;
}

static int cdata_address_like (CData *cd) {
  if (cd == NULL || (cd->flags & CDATA_CTYPE) != 0 || cd->ct == NULL)
    return 0;
  return cd->ct->kind == CT_POINTER || cd->ct->kind == CT_ARRAY ||
         cd->ct->kind == CT_FUNC;
}

static int meta_push (lua_State *L, CType *ct, const char *name) {
  /* LUAR: a plain struct/union has no metatype.  LUA_NOREF is not a valid
  ** registry index; indexing it produced nil and then made lua_getfield
  ** raise "attempt to index a nil value" from __len/__pairs instead of the
  ** intended cdata error. */
  if (ct == NULL || ct->metatype_ref == LUA_NOREF)
    return 0;
  lua_rawgeti(L, LUA_REGISTRYINDEX, ct->metatype_ref);
  lua_getfield(L, -1, name);
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 0;
  }
  lua_remove(L, -2);   /* keep the function only */
  return 1;
}


static int meta_binop (lua_State *L, const char *name) {
  CData *a = ffi_tocdata(L, 1);
  CData *b = ffi_tocdata(L, 2);
  if (!meta_push(L, cdata_metatype(a), name) &&
      !meta_push(L, cdata_metatype(b), name))
    return 0;
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_call(L, 2, 1);
  return 1;
}


static int cdata_eq (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *a = ffi_tocdata(L, 1);
  CData *b = ffi_tocdata(L, 2);
  if (meta_binop(L, "__eq"))
    return 1;
  /* The VM now routes an identical cdata pair here so that a metatype
  ** handler can observe self-comparison.  Without a handler, identity is
  ** still true for every cdata kind (structs and unions included). */
  if (a != NULL && a == b) {
    lua_pushboolean(L, 1);
    return 1;
  }
  if (a != NULL && b != NULL) {
    int actype = (a->flags & CDATA_CTYPE) != 0;
    int bctype = (b->flags & CDATA_CTYPE) != 0;
    if (actype || bctype) {
      /* A CType object denotes a type; ordinary cdata *is* a value of that
      ** type.  LuaJIT keeps those identities separate, and two CType
      ** objects of the same interned type still compare equal. */
      CType *ta = a->ct, *tb = b->ct;
      lua_pushboolean(L, actype && bctype &&
                         (ta == tb ||
                          (ta != NULL && tb != NULL && ta->id == tb->id)));
      return 1;
    }
    if ((a->ct->kind == CT_POINTER || a->ct->kind == CT_FUNC) &&
        (b->ct->kind == CT_POINTER || b->ct->kind == CT_FUNC)) {
      lua_pushboolean(L, a->v.p == b->v.p);
      return 1;
    }
    /* An array decays to a pointer to its first element for equality
    ** against any other pointer-like cdata.  Two distinct arrays are
    ** distinct objects, but an array and a pointer to its first element
    ** (or two references to the same storage) compare equal. */
    if ((cdata_is_array(a) || cdata_is_array(b)) &&
        cdata_address_like(a) && cdata_address_like(b)) {
      lua_pushboolean(L, a->v.p == b->v.p);
      return 1;
    }
  }
  else if (a != NULL || b != NULL) {
    CData *cd = (a != NULL) ? a : b;
    int other = (a != NULL) ? 2 : 1;
    if ((cd->flags & CDATA_CTYPE) != 0) {
      lua_pushboolean(L, 0);
      return 1;
    }
    if (lua_isnil(L, other)) {
      /* LuaJIT: NULL pointer/function cdata compares equal to nil;
      ** zero scalars and arrays do not. */
      if (cd->ct->kind == CT_POINTER || cd->ct->kind == CT_FUNC)
        lua_pushboolean(L, cd->v.p == NULL);
      else
        lua_pushboolean(L, 0);
      return 1;
    }
    if (lua_type(L, other) != LUA_TNUMBER) {
      lua_pushboolean(L, 0);
      return 1;
    }
  }
  else {
    lua_pushboolean(L, 0);
    return 1;
  }
  {
    int cmp;
    if (numeric_compare(L, S, 1, 2, &cmp)) {
      lua_pushboolean(L, cmp == 0);
      return 1;
    }
  }
  lua_pushboolean(L, 0);
  return 1;
}


static int pointer_base_compatible (CType *a, CType *b);

/* Ordering where at least one operand is an array: the array decays to a
** pointer, so the base types must be compatible (LuaJIT rejects
** 'int[2] < char *' and 'int[2] < void *').  Function pointers have no
** compatible base type and keep their separate identity/equality rules. */
static int array_order_compare (lua_State *L, CData *a, CData *b, int le) {
  CType *abase, *bbase;
  if (a == NULL || b == NULL || !cdata_address_like(a) ||
      !cdata_address_like(b) || a->ct->kind == CT_FUNC ||
      b->ct->kind == CT_FUNC)
    luaL_error(L, "ffi: attempt to compare cdata values");
  abase = (a->ct->kind == CT_ARRAY || a->ct->kind == CT_POINTER)
              ? a->ct->child : a->ct;
  bbase = (b->ct->kind == CT_ARRAY || b->ct->kind == CT_POINTER)
              ? b->ct->child : b->ct;
  if (!pointer_base_compatible(abase, bbase))
    luaL_error(L, "ffi: attempt to compare cdata values");
  lua_pushboolean(L, le ? (uintptr_t)a->v.p <= (uintptr_t)b->v.p
                        : (uintptr_t)a->v.p < (uintptr_t)b->v.p);
  return 1;
}

static int cdata_lt (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *a = ffi_tocdata(L, 1);
  CData *b = ffi_tocdata(L, 2);
  if (meta_binop(L, "__lt"))
    return 1;
  if (cdata_is_array(a) || cdata_is_array(b))
    return array_order_compare(L, a, b, 0);
  if (a != NULL && b != NULL &&
      (a->ct->kind == CT_POINTER || a->ct->kind == CT_FUNC) &&
      (b->ct->kind == CT_POINTER || b->ct->kind == CT_FUNC)) {
    /* Function pointers have no pointee type and compare by address;
    ** mixing them with object pointers is rejected, and object pointers
    ** need compatible base types, as in LuaJIT. */
    if (a->ct->kind == CT_FUNC || b->ct->kind == CT_FUNC) {
      if (a->ct->kind != b->ct->kind)
        luaL_error(L, "ffi: attempt to compare cdata values");
    }
    else if (!pointer_base_compatible(a->ct->child, b->ct->child))
      luaL_error(L, "ffi: attempt to compare cdata values");
    lua_pushboolean(L, (uintptr_t)a->v.p < (uintptr_t)b->v.p);
    return 1;
  }
  {
    int cmp;
    if (numeric_compare(L, S, 1, 2, &cmp)) {
      lua_pushboolean(L, cmp < 0);
      return 1;
    }
  }
  luaL_error(L, "ffi: attempt to compare cdata values");
  return 0;
}


static int cdata_le (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *a = ffi_tocdata(L, 1);
  CData *b = ffi_tocdata(L, 2);
  if (meta_binop(L, "__le"))
    return 1;
  if (cdata_is_array(a) || cdata_is_array(b))
    return array_order_compare(L, a, b, 1);
  if (a != NULL && b != NULL &&
      (a->ct->kind == CT_POINTER || a->ct->kind == CT_FUNC) &&
      (b->ct->kind == CT_POINTER || b->ct->kind == CT_FUNC)) {
    if (a->ct->kind == CT_FUNC || b->ct->kind == CT_FUNC) {
      if (a->ct->kind != b->ct->kind)
        luaL_error(L, "ffi: attempt to compare cdata values");
    }
    else if (!pointer_base_compatible(a->ct->child, b->ct->child))
      luaL_error(L, "ffi: attempt to compare cdata values");
    lua_pushboolean(L, (uintptr_t)a->v.p <= (uintptr_t)b->v.p);
    return 1;
  }
  {
    int cmp;
    if (numeric_compare(L, S, 1, 2, &cmp)) {
      lua_pushboolean(L, cmp <= 0);
      return 1;
    }
  }
  luaL_error(L, "ffi: attempt to compare cdata values");
  return 0;
}


static int numeric_arg (lua_State *L, FFIState *S, int idx,
                        int64_t *out, int *ok) {
  CData *cd = ffi_tocdata(L, idx);
  if (cd != NULL) {
    if ((cd->flags & CDATA_CTYPE) != 0 || !ffi_is_numeric(cd->ct)) {
      *ok = 0;
      return 0;
    }
    *out = ffi_to_int(L, S, idx, ok);
    return *ok;
  }
  if (lua_type(L, idx) == LUA_TNUMBER) {
    if (lua_isinteger(L, idx))
      *out = (int64_t)lua_tointeger(L, idx);
    else {
      /* LuaJIT accepts a floating offset and truncates it towards zero.
      ** Out-of-range/NaN/infinite values behave like a zero offset instead
      ** of invoking undefined integer conversion. */
      lua_Number n = lua_tonumber(L, idx);
      if (n >= -9223372036854775808.0 && n < 9223372036854775808.0)
        *out = (int64_t)n;
      else
        *out = 0;
    }
    *ok = 1;
    return 1;
  }
  *ok = 0;
  return 0;
}


static int push_pointer_result (lua_State *L, FFIState *S, CType *pt,
                                void *p) {
  CData *cd = ffi_push_scalar(L, S, pt);
  cd->v.p = p;
  return 1;
}


static CType *int64_type (FFIState *S) {
  if (S->t_long->size == 8)
    return S->t_long;
  return S->t_llong;
}


static CType *uint64_type (FFIState *S) {
  if (S->t_ulong->size == 8)
    return S->t_ulong;
  return S->t_ullong;
}


/* C pointer arithmetic is defined only for pointers to complete object
** types: 'void *' and pointers to incomplete structs/unions have no
** element size.  LuaJIT rejects both; accepting them with a one-byte
** element size silently turns a type error into byte arithmetic. */
static size_t pointer_elem_size (lua_State *L, FFIState *S, CType *pt) {
  size_t esz = ffi_type_size(pt->child);
  if (esz == FFI_SIZE_UNKNOWN || esz == 0)
    luaL_error(L, "ffi: pointer arithmetic on '%s'", ffi_type_repr(S, pt));
  return esz;
}


/* LuaJIT rejects 'p - q' when the two pointers do not have compatible
** base types (for example 'int *' and 'char *', or two different struct
** tags).  Integer types with the same size are compatible regardless of
** signedness; enums behave like their underlying integer type; structs
** and unions need the same interned type. */
static int pointer_base_compatible (CType *a, CType *b) {
  int aint, bint;
  if (a == b)
    return 1;
  aint = (a->kind == CT_INT || a->kind == CT_ENUM);
  bint = (b->kind == CT_INT || b->kind == CT_ENUM);
  if (a->kind != b->kind) {
    if (aint && bint)
      return a->size == b->size;
    return 0;
  }
  switch (a->kind) {
    case CT_VOID:
    case CT_BOOL:
      return 1;
    case CT_INT:
    case CT_ENUM:
    case CT_FLOAT:
      return a->size == b->size;
    case CT_POINTER:
      return pointer_base_compatible(a->child, b->child);
    case CT_ARRAY:
      return a->length == b->length &&
             pointer_base_compatible(a->child, b->child);
    default:   /* struct, union, function */
      return 0;
  }
}


static int cdata_arith (lua_State *L, int op) {
  FFIState *S = ffi_getstate(L);
  CData *a = ffi_tocdata(L, 1);
  CData *b = ffi_tocdata(L, 2);
  int aptr = a != NULL && (a->flags & CDATA_CTYPE) == 0 &&
             (a->ct->kind == CT_POINTER || a->ct->kind == CT_ARRAY);
  int bptr = b != NULL && (b->flags & CDATA_CTYPE) == 0 &&
             (b->ct->kind == CT_POINTER || b->ct->kind == CT_ARRAY);
  if (aptr || bptr) {
    if (aptr && bptr) {
      if (op != '-')
        luaL_error(L, "ffi: invalid pointer arithmetic");
      if (a->ct->child->kind == CT_FUNC || b->ct->child->kind == CT_FUNC)
        luaL_error(L, "ffi: pointer arithmetic on a function pointer");
      if (!pointer_base_compatible(a->ct->child, b->ct->child))
        luaL_error(L, "ffi: pointer subtraction requires compatible types");
      {
        size_t esz = pointer_elem_size(L, S, a->ct);
        ptrdiff_t diff = (char *)a->v.p - (char *)b->v.p;
        /* LuaJIT returns a Lua integer here, not an int64_t cdata. */
        lua_pushinteger(L, (lua_Integer)(diff / (ptrdiff_t)esz));
        return 1;
      }
    }
    {
      CData *pcd = aptr ? a : b;
      int idx = aptr ? 2 : 1;
      int64_t n = 0;
      int ok;
      size_t esz;
      char *p;
      if (op == '-') {
        if (!aptr)
          luaL_error(L, "ffi: invalid pointer arithmetic");
      }
      else if (op != '+') {
        luaL_error(L, "ffi: invalid pointer arithmetic");
      }
      if (pcd->ct->child->kind == CT_FUNC)
        luaL_error(L, "ffi: pointer arithmetic on a function pointer");
      numeric_arg(L, S, idx, &n, &ok);
      if (!ok)
        luaL_error(L, "ffi: pointer arithmetic requires an integer offset");
      esz = pointer_elem_size(L, S, pcd->ct);
      if (op == '-')
        p = (char *)pcd->v.p - (ptrdiff_t)n * (ptrdiff_t)esz;
      else
        p = (char *)pcd->v.p + (ptrdiff_t)n * (ptrdiff_t)esz;
      /* An array decays to a pointer to its element type; the result of
      ** array arithmetic is a pointer, never a new array object. */
      if (pcd->ct->kind == CT_ARRAY)
        return push_pointer_result(L, S,
                                   ffi_type_ptr(L, S, pcd->ct->child), p);
      return push_pointer_result(L, S, pcd->ct, p);
    }
  }
  {
    int uns = cdata_is_unsigned64(a) || cdata_is_unsigned64(b);
    if (!numeric_operand(L, 1) || !numeric_operand(L, 2))
      luaL_error(L, "ffi: invalid operands for C arithmetic");
    if (uns) {
      int ok1, ok2;
      uint64_t x = ffi_to_uint(L, S, 1, &ok1);
      uint64_t y = ffi_to_uint(L, S, 2, &ok2);
      uint64_t z = 0;
      CData *r;
      if (!ok1 || !ok2)
        luaL_error(L, "ffi: invalid integer arithmetic");
      switch (op) {
        case '+': z = x + y; break;
        case '-': z = x - y; break;
        case '*': z = x * y; break;
        case '/': z = (y == 0) ? ((uint64_t)1 << 63) : x / y; break;
        case '%': z = (y == 0) ? ((uint64_t)1 << 63) : x % y; break;
        default: z = 0;
      }
      r = ffi_push_scalar(L, S, uint64_type(S));
      ffi_store_uint(&r->v, 8, z);
      return 1;
    }
    else {
      int ok1, ok2;
      int64_t x = ffi_to_int(L, S, 1, &ok1);
      int64_t y = ffi_to_int(L, S, 2, &ok2);
      int64_t z = 0;
      CData *r;
      if (!ok1 || !ok2)
        luaL_error(L, "ffi: invalid integer arithmetic");
      switch (op) {
        case '+': z = (int64_t)((uint64_t)x + (uint64_t)y); break;
        case '-': z = (int64_t)((uint64_t)x - (uint64_t)y); break;
        case '*': z = (int64_t)((uint64_t)x * (uint64_t)y); break;
        case '/':
          z = (y == 0 || (x == INT64_MIN && y == -1))
                  ? INT64_MIN : x / y;
          break;
        case '%':
          z = (y == 0) ? INT64_MIN
                       : (x == INT64_MIN && y == -1) ? 0 : x % y;
          break;
        default: z = 0;
      }
      r = ffi_push_scalar(L, S, int64_type(S));
      ffi_store_int(&r->v, 8, z);
      return 1;
    }
  }
}


/* LUAR: a metatype may override arithmetic for struct/union cdata, as in
** LuaJIT.  Numeric cdata has no metatype and keeps the FFI fast paths. */
static int cdata_add (lua_State *L) {
  if (meta_binop(L, "__add")) return 1;
  return cdata_arith(L, '+');
}
static int cdata_sub (lua_State *L) {
  if (meta_binop(L, "__sub")) return 1;
  return cdata_arith(L, '-');
}
static int cdata_mul (lua_State *L) {
  if (meta_binop(L, "__mul")) return 1;
  return cdata_arith(L, '*');
}
static int cdata_div (lua_State *L) {
  if (meta_binop(L, "__div")) return 1;
  return cdata_arith(L, '/');
}
static int cdata_mod (lua_State *L) {
  if (meta_binop(L, "__mod")) return 1;
  return cdata_arith(L, '%');
}


static int cdata_unm (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *a = ffi_tocdata(L, 1);
  CData *r;
  if (meta_push(L, cdata_metatype(a), "__unm")) {
    /* The VM passes a unary operand as both arguments, so the handler
    ** observes the same pair under LuaJIT. */
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 1);
    lua_call(L, 2, 1);
    return 1;
  }
  if (a == NULL || (a->flags & CDATA_CTYPE) != 0 ||
      !ffi_is_numeric(a->ct))
    luaL_error(L, "ffi: invalid operand for unary minus");
  if (cdata_is_unsigned64(a)) {
    uint64_t u = ffi_to_uint(L, S, 1, NULL);
    r = ffi_push_scalar(L, S, uint64_type(S));
    ffi_store_uint(&r->v, 8, (uint64_t)(0 - u));
  }
  else {
    int64_t v = ffi_to_int(L, S, 1, NULL);
    r = ffi_push_scalar(L, S, int64_type(S));
    ffi_store_int(&r->v, 8, (int64_t)(0 - (uint64_t)v));
  }
  return 1;
}


static uint64_t pow_u64 (uint64_t x, uint64_t y) {
  uint64_t r = 1;
  while (y != 0) {
    if ((y & 1) != 0)
      r *= x;
    y >>= 1;
    if (y != 0)
      x *= x;
  }
  return r;
}


static int64_t pow_i64 (int64_t x, int64_t y) {
  if (y < 0) {
    if (x == 1) return 1;
    if (x == -1) return ((y & 1) != 0) ? -1 : 1;
    /* 0 ** negative is +infinity.  LuaJIT converts that to the int64
    ** sentinel INT64_MAX; returning zero silently changed the result. */
    if (x == 0) return INT64_MAX;
    return 0;
  }
  return (int64_t)pow_u64((uint64_t)x, (uint64_t)y);
}


/* Bitwise and shift operators on cdata.  LuaJIT semantics: 64-bit
** two's-complement results, int64/uint64 cdata output, shift counts use
** the low six bits, right shifts are logical. */
static int cdata_bitwise (lua_State *L, int op) {
  FFIState *S = ffi_getstate(L);
  CData *a = ffi_tocdata(L, 1);
  int two = (op != '~');
  CData *b = two ? ffi_tocdata(L, 2) : NULL;
  int uns;
  int ok1 = 1, ok2 = 1;
  uint64_t x, y = 0, z = 0;
  CData *r;

  if (!numeric_operand(L, 1) || (two && !numeric_operand(L, 2)))
    return luaL_error(L, "ffi: invalid operands for C bitwise operation");

  /* '&', '|' and '^' are unsigned if either operand is unsigned 64-bit;
  ** shifts follow the left operand, as in LuaJIT. */
  uns = cdata_is_unsigned64(a);
  if (two && (op == '&' || op == '|' || op == '^'))
    uns = uns || cdata_is_unsigned64(b);

  x = uns ? ffi_to_uint(L, S, 1, &ok1)
          : (uint64_t)ffi_to_int(L, S, 1, &ok1);
  if (!ok1)
    return luaL_error(L, "ffi: cannot convert operand to an integer");
  if (two) {
    y = uns ? ffi_to_uint(L, S, 2, &ok2)
            : (uint64_t)ffi_to_int(L, S, 2, &ok2);
    if (!ok2)
      return luaL_error(L, "ffi: cannot convert operand to an integer");
  }

  switch (op) {
    case '&': z = x & y; break;
    case '|': z = x | y; break;
    case '^': z = x ^ y; break;
    case '~': z = ~x; break;
    case 'L': z = x << (y & 63); break;
    case 'R': z = x >> (y & 63); break;
    default: return luaL_error(L, "ffi: bad bitwise operation");
  }

  if (uns) {
    r = ffi_push_scalar(L, S, uint64_type(S));
    ffi_store_uint(&r->v, 8, z);
  }
  else {
    r = ffi_push_scalar(L, S, int64_type(S));
    ffi_store_int(&r->v, 8, (int64_t)z);
  }
  return 1;
}


static int cdata_band (lua_State *L) {
  if (meta_binop(L, "__band")) return 1;
  return cdata_bitwise(L, '&');
}
static int cdata_bor (lua_State *L) {
  if (meta_binop(L, "__bor")) return 1;
  return cdata_bitwise(L, '|');
}
static int cdata_bxor (lua_State *L) {
  if (meta_binop(L, "__bxor")) return 1;
  return cdata_bitwise(L, '^');
}
static int cdata_bnot (lua_State *L) {
  CData *a = ffi_tocdata(L, 1);
  if (meta_push(L, cdata_metatype(a), "__bnot")) {
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 1);
    lua_call(L, 2, 1);
    return 1;
  }
  return cdata_bitwise(L, '~');
}
static int cdata_shl (lua_State *L) {
  if (meta_binop(L, "__shl")) return 1;
  return cdata_bitwise(L, 'L');
}
static int cdata_shr (lua_State *L) {
  if (meta_binop(L, "__shr")) return 1;
  return cdata_bitwise(L, 'R');
}


static int cdata_idiv (lua_State *L) {
  if (meta_binop(L, "__idiv")) return 1;
  return cdata_arith(L, '/');
}


static int cdata_pow (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  if (meta_binop(L, "__pow")) return 1;
  CData *a = ffi_tocdata(L, 1);
  CData *b = ffi_tocdata(L, 2);
  int uns;
  int ok1 = 1, ok2 = 1;
  CData *r;

  if (!numeric_operand(L, 1) || !numeric_operand(L, 2))
    return luaL_error(L, "ffi: invalid operands for C arithmetic");
  uns = cdata_is_unsigned64(a) || cdata_is_unsigned64(b);
  if (uns) {
    uint64_t x = ffi_to_uint(L, S, 1, &ok1);
    uint64_t y = ffi_to_uint(L, S, 2, &ok2);
    if (!ok1 || !ok2)
      return luaL_error(L, "ffi: invalid integer arithmetic");
    r = ffi_push_scalar(L, S, uint64_type(S));
    ffi_store_uint(&r->v, 8, pow_u64(x, y));
  }
  else {
    int64_t x = ffi_to_int(L, S, 1, &ok1);
    int64_t y = ffi_to_int(L, S, 2, &ok2);
    if (!ok1 || !ok2)
      return luaL_error(L, "ffi: invalid integer arithmetic");
    r = ffi_push_scalar(L, S, int64_type(S));
    ffi_store_int(&r->v, 8, pow_i64(x, y));
  }
  return 1;
}


static int cdata_call (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *cd = ffi_checkcdata(L, 1, "__call");
  CType *ct = cd->ct;
  if ((cd->flags & CDATA_CTYPE) != 0) {
    /* LUAR: LuaJIT lets a metatype override construction through __new;
    ** only calling the CType object goes through it.  ffi.new/ffi.cast
    ** keep the built-in constructor, so a handler may call them without
    ** recursing.  The handler receives the type object first. */
    CType *mct = meta_ctype(ct);
    int nargs = lua_gettop(L);
    if (mct != NULL && meta_push(L, mct, "__new")) {
      lua_insert(L, 1);              /* handler before the arguments */
      lua_call(L, nargs, LUA_MULTRET);
      return lua_gettop(L);
    }
    return ffi_do_new(L, S, ct, 2);
  }
  if (ct->kind == CT_FUNC ||
      (ct->kind == CT_POINTER && ct->child != NULL &&
       ct->child->kind == CT_FUNC))
    return ffi_call_cdata(L, S, cd, 2);
  {
    CType *mct = meta_ctype(ct);
    if (mct != NULL && mct->metatype_ref != LUA_NOREF) {
      int nargs = lua_gettop(L);
      lua_rawgeti(L, LUA_REGISTRYINDEX, mct->metatype_ref);
      lua_getfield(L, -1, "__call");
      if (lua_isfunction(L, -1)) {
        lua_remove(L, -2);
        lua_insert(L, 1);
        lua_call(L, nargs, LUA_MULTRET);
        return lua_gettop(L);
      }
      lua_pop(L, 2);
    }
  }
  luaL_error(L, "ffi: attempt to call a '%s' cdata",
             ffi_type_repr(S, ct));
  return 0;
}


static int cdata_gc (lua_State *L) {
  CData *cd;
  if (lua_type(L, 1) != LUA_TUSERDATA)
    return 0;
  if ((size_t)lua_rawlen(L, 1) < sizeof(CData))
    return 0;
  cd = (CData *)lua_touserdata(L, 1);
  if (cd == NULL || cd->magic != LUAR_FFI_MAGIC)
    return 0;
  lua_getiuservalue(L, 1, 2);
  if (lua_isfunction(L, -1)) {
    lua_pushnil(L);
    lua_setiuservalue(L, 1, 2);
    lua_pushvalue(L, 1);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
      lua_pop(L, 1);
  }
  else {
    CData *fn = ffi_tocdata(L, -1);
    int is_cfunc = fn != NULL && (fn->flags & CDATA_CTYPE) == 0 &&
                   (fn->ct->kind == CT_FUNC ||
                    (fn->ct->kind == CT_POINTER && fn->ct->child != NULL &&
                     fn->ct->child->kind == CT_FUNC));
    if (is_cfunc && fn->v.p != NULL) {
      FFIState *S = cd->S;
      lua_pushnil(L);
      lua_setiuservalue(L, 1, 2);	/* clear before calling */
      lua_pushvalue(L, 1);		/* the cdata argument */
      ffi_call_cdata(L, S, fn, 3);
      lua_settop(L, 1);
    }
    else {
      lua_pop(L, 1);
    }
  }
  /* LUAR: a metatype __gc is the default finalizer, but an explicit
  ** ffi.gc() call (including ffi.gc(x, nil)) overrides it, as in
  ** LuaJIT.  The closure-free path is exactly the previous behaviour. */
  if ((cd->flags & CDATA_GC_SET) == 0 &&
      meta_push(L, cdata_metatype(cd), "__gc")) {
    lua_pushvalue(L, 1);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
      lua_pop(L, 1);
  }
  if ((cd->flags & CDATA_CALLBACK) != 0)
    ffi_callback_free(L, cd);
  return 0;
}


/* ------------------------------------------------------------------ */
/* ffi introspection functions                                       */
/* ------------------------------------------------------------------ */

static int ffi_typeof (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *cd;
  if (lua_type(L, 1) == LUA_TSTRING) {
    size_t len;
    const char *s = lua_tolstring(L, 1, &len);
    CType *ct = ffi_parse_type_cached(L, S, s, len);
    ffi_push_ctype(L, S, ct);
    return 1;
  }
  cd = ffi_tocdata(L, 1);
  if (cd == NULL)
    luaL_argerror(L, 1, "C type or cdata expected");
  ffi_push_ctype(L, S, cd->ct);
  return 1;
}


static int ctype_istype (CType *ct, CType *obj) {
  if (ct == obj)
    return 1;
  if ((ct->kind == CT_STRUCT || ct->kind == CT_UNION) &&
      obj->kind == CT_POINTER)
    return obj->child == ct;
  if (ct->kind == CT_POINTER && obj->kind == CT_POINTER)
    return ct->child == obj->child;
  return 0;
}


static int ffi_istype (lua_State *L) {
  CType *ct = ffi_checkctype(L, 1, "istype");
  CData *cd = ffi_tocdata(L, 2);
  lua_pushboolean(L, cd != NULL && ctype_istype(ct, cd->ct));
  return 1;
}


static int ffi_sizeof (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *raw = ffi_tocdata(L, 1);
  CType *ct = ffi_checkctype(L, 1, "sizeof");
  size_t sz = FFI_SIZE_UNKNOWN;
  if (ct->kind == CT_ARRAY && ct->vla) {
    size_t n;
    size_t esz = ffi_type_size(ct->child);
    if (raw != NULL && (raw->flags & CDATA_CTYPE) == 0 &&
        raw->ct->kind == CT_ARRAY && raw->ct->vla) {
      n = raw->len;
    }
    else {
      if (lua_isnoneornil(L, 2))
        luaL_error(L, "ffi: size of variable-length array is unknown "
                      "(pass the length as a second argument)");
      n = check_len_arg(L, 2, "array length");
    }
    if (esz == FFI_SIZE_UNKNOWN)
      luaL_error(L, "ffi: array element has unknown size");
    if (n != 0 && esz > FFI_SIZE_UNKNOWN / n)
      luaL_error(L, "ffi: size overflow");
    sz = esz * n;
  }
  else if (ct->kind == CT_VOID || ct->kind == CT_FUNC ||
           ct->incomplete) {
    luaL_error(L, "ffi: size of C type '%s' is unknown or too large",
               ffi_type_repr(S, ct));
  }
  else {
    sz = ffi_type_size(ct);
  }
  if (sz == FFI_SIZE_UNKNOWN || sz > (size_t)LUA_MAXINTEGER)
    luaL_error(L, "ffi: size of C type '%s' is unknown or too large",
               ffi_type_repr(S, ct));
  lua_pushinteger(L, (lua_Integer)sz);
  return 1;
}


static int ffi_alignof (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CType *ct = ffi_checkctype(L, 1, "alignof");
  if (ct->kind == CT_VOID || ct->kind == CT_FUNC)
    luaL_error(L, "ffi: alignment of '%s' is unknown",
               ffi_type_repr(S, ct));
  lua_pushinteger(L, (lua_Integer)ffi_type_align(ct));
  return 1;
}


static int ffi_offsetof (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CType *ct = ffi_checkctype(L, 1, "offsetof");
  const char *name = luaL_checkstring(L, 2);
  const char *p = name;
  size_t off = 0;
  CType *cur = ct;
  CField *field = NULL;
  if (ct->kind != CT_STRUCT && ct->kind != CT_UNION)
    luaL_error(L, "ffi: offsetof requires a struct or union type");
  for (;;) {
    char fname[256];
    size_t n = 0, member_off = 0;
    CType *field_type = NULL;
    while (*p != '\0' && *p != '.' && *p != '[') {
      if (n + 1 >= sizeof(fname))
        luaL_error(L, "ffi: offsetof field name is too long");
      fname[n++] = *p++;
    }
    fname[n] = '\0';
    if (n == 0)
      luaL_error(L, "ffi: expected a field name in offsetof");
    field = ffi_type_find_field(cur, fname, &member_off, &field_type);
    if (field == NULL)
      luaL_error(L, "ffi: '%s' has no member named '%s'",
                 ffi_type_repr(S, cur), fname);
    off += member_off;
    cur = field_type;
    while (*p == '[') {
      char *end = NULL;
      unsigned long index;
      size_t esz;
      if (cur == NULL || cur->kind != CT_ARRAY || cur->vla)
        luaL_error(L, "ffi: offsetof '[]' needs an array member");
      p++;
      index = strtoul(p, &end, 10);
      if (end == p || *end != ']')
        luaL_error(L, "ffi: malformed array index in offsetof");
      esz = ffi_type_size(cur->child);
      if (esz == FFI_SIZE_UNKNOWN)
        luaL_error(L, "ffi: offsetof of an incomplete array element");
      off += (size_t)index * esz;
      cur = cur->child;
      p = end + 1;
    }
    if (*p == '.') {
      p++;
      continue;
    }
    if (*p != '\0')
      luaL_error(L, "ffi: malformed member name in offsetof");
    break;
  }
  if (field->bitwidth != 0) {
    size_t usize = ffi_type_size(field->type);
    size_t basebits = (usize != 0 && usize != FFI_SIZE_UNKNOWN)
                          ? usize * 8 : 0;
    /* LuaJIT reports a bitfield that fills its base type and starts on a
    ** storage boundary like an ordinary member: one byte offset and no bit
    ** position.  A full-width field that starts inside another bitfield is
    ** still a bitfield (for example `unsigned long long b:64` after a one-
    ** bit field). */
    if (basebits != 0 && field->bitwidth >= basebits &&
        field->bitoffset == 0) {
      lua_pushinteger(L, (lua_Integer)off);
      return 1;
    }
    /* A packed bitfield of at most one byte is reported relative to its
    ** containing byte.  Wider packed fields (which may cross bytes) use
    ** the base type's storage-unit offset, exactly like LuaJIT. */
    if (field->packed && field->bitwidth <= 8) {
      lua_pushinteger(L, (lua_Integer)off);
      lua_pushinteger(L, (lua_Integer)field->bitoffset);
      lua_pushinteger(L, (lua_Integer)field->bitwidth);
      return 3;
    }
    {
      size_t unit = off;
      size_t bpos;
      if (usize != 0 && usize != FFI_SIZE_UNKNOWN)
        unit = (off / usize) * usize;
      bpos = (off - unit) * 8 + field->bitoffset;
      lua_pushinteger(L, (lua_Integer)unit);
      lua_pushinteger(L, (lua_Integer)bpos);
      lua_pushinteger(L, (lua_Integer)field->bitwidth);
      return 3;
    }
  }
  lua_pushinteger(L, (lua_Integer)off);
  return 1;
}


static int host_is_little_endian (void) {
  uint16_t x = 1;
  unsigned char c;
  memcpy(&c, &x, 1);
  return c == 1;
}


static int ffi_lib_abi (lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  int b = 0;
  if (strcmp(s, "64bit") == 0) b = sizeof(void *) == 8;
  else if (strcmp(s, "32bit") == 0) b = sizeof(void *) == 4;
  else if (strcmp(s, "le") == 0) b = host_is_little_endian();
  else if (strcmp(s, "be") == 0) b = !host_is_little_endian();
  else if (strcmp(s, "fpu") == 0) b = 1;
  else if (strcmp(s, "hardfp") == 0) b = 1;
  else if (strcmp(s, "softfp") == 0) b = 0;
  else if (strcmp(s, "gc64") == 0) b = (sizeof(void *) == 8);
  else b = 0;
  lua_pushboolean(L, b);
  return 1;
}


/* ------------------------------------------------------------------ */
/* ffi string/memory helpers                                         */
/* ------------------------------------------------------------------ */

static void *mem_arg (lua_State *L, FFIState *S, CData *cd,
                      const char *fname, size_t *avail) {
  CType *ct = cd->ct;
  if ((cd->flags & CDATA_CTYPE) != 0)
    luaL_error(L, "ffi.%s: expected a pointer or array cdata", fname);
  if (ct->kind == CT_POINTER) {
    *avail = FFI_SIZE_UNKNOWN;
    return cd->v.p;
  }
  if (ct->kind == CT_ARRAY) {
    *avail = ffi_cdata_size(cd);
    return cd->v.p;
  }
  luaL_error(L, "ffi.%s: expected a pointer or array cdata, got '%s'",
             fname, ffi_type_repr(S, ct));
  return NULL;
}


static int ffi_string (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *cd = ffi_checkcdata(L, 1, "string");
  CType *ct = cd->ct;
  void *p;
  size_t avail;
  size_t len = 0;
  int haslen = !lua_isnoneornil(L, 2);
  if (ct->kind == CT_POINTER) {
    if (!ctype_char_like(ct->child))
      luaL_error(L, "ffi.string: '%s' is not a char pointer",
                 ffi_type_repr(S, ct));
    p = cd->v.p;
    avail = FFI_SIZE_UNKNOWN;
  }
  else if (ct->kind == CT_ARRAY && ctype_char_like(ct->child)) {
    p = value_addr(cd);
    avail = ffi_cdata_size(cd);
  }
  else {
    luaL_error(L, "ffi.string: expected a char pointer or array, got '%s'",
               ffi_type_repr(S, ct));
    return 0;
  }
  if (haslen) {
    len = check_len_arg(L, 2, "string length");
    if (avail != FFI_SIZE_UNKNOWN && len > avail)
      luaL_error(L, "ffi.string: length out of bounds");
  }
  if (p == NULL) {
    if (haslen && len == 0) {
      lua_pushliteral(L, "");
      return 1;
    }
    luaL_error(L, "ffi.string: NULL pointer");
  }
  if (!haslen) {
    if (avail != FFI_SIZE_UNKNOWN) {
      const void *z = memchr(p, '\0', avail);
      len = (z != NULL) ? (size_t)((const char *)z - (const char *)p)
                        : avail;
    }
    else {
      len = strlen((const char *)p);
    }
  }
  lua_pushlstring(L, (const char *)p, len);
  return 1;
}


static int ffi_copy (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *dst = ffi_checkcdata(L, 1, "copy");
  size_t davail;
  void *dp = mem_arg(L, S, dst, "copy", &davail);
  int src_is_string = lua_type(L, 2) == LUA_TSTRING;
  size_t len;
  const char *s = NULL;
  size_t slen = 0;
  void *sp = NULL;
  size_t savail = FFI_SIZE_UNKNOWN;
  if (src_is_string) {
    s = lua_tolstring(L, 2, &slen);
    if (lua_isnoneornil(L, 3))
      len = slen + 1;		/* LuaJIT: copy the trailing NUL too */
    else
      len = check_len_arg(L, 3, "copy length");
    if (len > slen + 1)
      luaL_error(L, "ffi.copy: source string is shorter than the length");
  }
  else {
    if (lua_isnoneornil(L, 3))
      luaL_error(L, "ffi.copy: length required when the source is a cdata");
    len = check_len_arg(L, 3, "copy length");
    /* LUAR: validate the source even for a zero-length copy.  The old
    ** early return accepted a scalar/table/number source silently, while
    ** LuaJIT rejects anything that is not a pointer/array (NULL is the one
    ** zero-length exception). */
    if (!(len == 0 && lua_isnil(L, 2))) {
      CData *src = ffi_checkcdata(L, 2, "copy");
      sp = mem_arg(L, S, src, "copy", &savail);
    }
  }
  if (davail != FFI_SIZE_UNKNOWN && len > davail)
    luaL_error(L, "ffi.copy: destination length out of bounds");
  if (len == 0)
    return 0;
  if (dp == NULL)
    luaL_error(L, "ffi.copy: NULL destination pointer");
  if (src_is_string) {
    memcpy(dp, s, len);		/* len may include the string's NUL */
    return 0;
  }
  if (savail != FFI_SIZE_UNKNOWN && len > savail)
    luaL_error(L, "ffi.copy: source length out of bounds");
  if (sp == NULL)
    luaL_error(L, "ffi.copy: NULL source pointer");
  memmove(dp, sp, len);
  return 0;
}


static int ffi_fill (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *dst = ffi_checkcdata(L, 1, "fill");
  size_t davail;
  void *dp = mem_arg(L, S, dst, "fill", &davail);
  size_t len = check_len_arg(L, 2, "fill length");
  lua_Integer c = check_c_int_arg(L, 3, "fill value", 1, 0);
  if (davail != FFI_SIZE_UNKNOWN && len > davail)
    luaL_error(L, "ffi.fill: length out of bounds");
  if (len == 0)
    return 0;
  if (dp == NULL)
    luaL_error(L, "ffi.fill: NULL pointer");
  memset(dp, c & 0xff, len);
  return 0;
}


/* ------------------------------------------------------------------ */
/* ffi.gc / ffi.metatype / ffi.errno                                 */
/* ------------------------------------------------------------------ */

static int ffi_gc (lua_State *L) {
  CData *cd = ffi_checkcdata(L, 1, "gc");
  if ((cd->flags & CDATA_CTYPE) != 0)
    luaL_error(L, "ffi.gc: cannot set a finalizer on a C type");
  cd->flags |= CDATA_GC_SET;
  if (lua_isnoneornil(L, 2)) {
    lua_pushnil(L);
    lua_setiuservalue(L, 1, 2);
  }
  else if (lua_isfunction(L, 2)) {
    lua_pushvalue(L, 2);
    lua_setiuservalue(L, 1, 2);
  }
  else {
    CData *fn = ffi_tocdata(L, 2);
    int is_cfunc = fn != NULL && (fn->flags & CDATA_CTYPE) == 0 &&
                   (fn->ct->kind == CT_FUNC ||
                    (fn->ct->kind == CT_POINTER && fn->ct->child != NULL &&
                     fn->ct->child->kind == CT_FUNC));
    if (!is_cfunc)
      luaL_argerror(L, 2, "Lua function or C function cdata expected");
    if (fn->v.p == NULL)
      luaL_argerror(L, 2, "NULL C function finalizer");
    lua_pushvalue(L, 2);
    lua_setiuservalue(L, 1, 2);
  }
  lua_pushvalue(L, 1);
  return 1;
}


static int ffi_metatype (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CType *ct = ffi_checkctype(L, 1, "metatype");
  luaL_checktype(L, 2, LUA_TTABLE);
  if (ct->kind != CT_STRUCT && ct->kind != CT_UNION)
    luaL_error(L, "ffi.metatype: only struct/union types can have "
                  "a metatype");
  /* LUAR: a type has exactly one metatype.  LuaJIT raises even when the
  ** same table is passed again; silently accepting a repeat made a second
  ** registration look successful while the first table stayed in effect.
  ** Reject every repeat so callers cannot believe they replaced it. */
  if (ct->metatype_ref != LUA_NOREF)
    luaL_error(L, "ffi.metatype: cannot change a protected metatable");
  lua_pushvalue(L, 2);
  ct->metatype_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ffi_push_ctype(L, S, ct);
  return 1;
}


static int ffi_errno (lua_State *L) {
  int old = errno;
  if (!lua_isnoneornil(L, 1))
    errno = (int)check_c_int_arg(L, 1, "errno", 0, 0);
  lua_pushinteger(L, old);
  return 1;
}


/* ------------------------------------------------------------------ */
/* ffi.cdef                                                          */
/* ------------------------------------------------------------------ */

static int ffi_cdef (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  int n = lua_gettop(L);
  int i;
  if (n < 1)
    luaL_error(L, "ffi.cdef: string expected");
  for (i = 1; i <= n; i++) {
    size_t len;
    const char *s = luaL_checklstring(L, i, &len);
    ffi_parse_cdef(L, S, s, len);
  }
  return 0;
}


/* ------------------------------------------------------------------ */
/* Lua type() wrapper (LuaJIT reports cdata)                          */
/* ------------------------------------------------------------------ */

static int ffi_lua_type (lua_State *L) {
  luaL_checkany(L, 1);
  if (ffi_iscdata(L, 1)) {
    lua_pushliteral(L, "cdata");
    return 1;
  }
  lua_getfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_type");
  if (lua_isfunction(L, -1)) {
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    return 1;
  }
  lua_pop(L, 1);
  lua_pushstring(L, luaL_typename(L, 1));
  return 1;
}


/* ------------------------------------------------------------------ */
/* Library registration                                              */
/* ------------------------------------------------------------------ */

static int ffi_callback (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CType *ct = ffi_checkctype(L, 1, "callback");
  luaL_checktype(L, 2, LUA_TFUNCTION);
  return ffi_callback_new(L, S, ct, 2);
}


static int ffi_callback_error (lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "luar.ffi.callback_error");
  lua_pushnil(L);
  lua_setfield(L, LUA_REGISTRYINDEX, "luar.ffi.callback_error");
  return 1;
}


static const luaL_Reg ffi_lib[] = {
  {"cdef", ffi_cdef},
  {"new", ffi_new},
  {"cast", ffi_cast},
  {"typeof", ffi_typeof},
  {"istype", ffi_istype},
  {"sizeof", ffi_sizeof},
  {"alignof", ffi_alignof},
  {"offsetof", ffi_offsetof},
  {"abi", ffi_lib_abi},
  {"string", ffi_string},
  {"copy", ffi_copy},
  {"fill", ffi_fill},
  {"gc", ffi_gc},
  {"metatype", ffi_metatype},
  {"errno", ffi_errno},
  {"load", ffi_load},
  {"callback", ffi_callback},
  {"callback_error", ffi_callback_error},
  {NULL, NULL}
};


static const luaL_Reg cdata_meta[] = {
  {"__index", cdata_index},
  {"__newindex", cdata_newindex},
  {"__call", cdata_call},
  {"__tostring", cdata_tostring},
  {"__eq", cdata_eq},
  {"__lt", cdata_lt},
  {"__le", cdata_le},
  {"__len", cdata_len},
  {"__pairs", cdata_pairs},
  {"__concat", cdata_concat},
  {"__add", cdata_add},
  {"__sub", cdata_sub},
  {"__mul", cdata_mul},
  {"__div", cdata_div},
  {"__mod", cdata_mod},
  {"__idiv", cdata_idiv},
  {"__pow", cdata_pow},
  {"__band", cdata_band},
  {"__bor", cdata_bor},
  {"__bxor", cdata_bxor},
  {"__bnot", cdata_bnot},
  {"__shl", cdata_shl},
  {"__shr", cdata_shr},
  {"__unm", cdata_unm},
  {"__gc", cdata_gc},
  {NULL, NULL}
};


static int state_gc (lua_State *L) {
  FFIState *S = (FFIState *)lua_touserdata(L, 1);
  if (S != NULL)
    ffi_state_free(S);
  return 0;
}


static void create_metatables (lua_State *L) {
  luaL_newmetatable(L, LUAR_FFI_CDATA_MT);
  luaL_setfuncs(L, cdata_meta, 0);
  lua_pushliteral(L, "cdata");
  lua_setfield(L, -2, "__name");
  /* Reserved key used by the VM to recognize FFI cdata with numbers
  ** (see the LUAR comment in lvm.c and doc/ffi.md). */
  lua_pushinteger(L, 0x4C554652);
  lua_pushboolean(L, 1);
  lua_settable(L, -3);
  lua_pop(L, 1);

  ffi_clib_initmt(L);

  luaL_newmetatable(L, LUAR_FFI_STATE_MT);
  lua_pushcfunction(L, state_gc);
  lua_setfield(L, -2, "__gc");
  lua_pop(L, 1);
}


static int ffi_lua_tonumber (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  CData *cd = ffi_tocdata(L, 1);
  if (cd != NULL && (cd->flags & CDATA_CTYPE) == 0 &&
      ffi_is_numeric(cd->ct)) {
    CType *ct = cd->ct;
    if (ct->kind == CT_FLOAT) {
      lua_pushnumber(L, (lua_Number)ffi_to_double(L, S, 1, NULL));
    }
    else if (ct->is_unsigned) {
      uint64_t u = ffi_to_uint(L, S, 1, NULL);
      if (u <= (uint64_t)LUA_MAXINTEGER)
        lua_pushinteger(L, (lua_Integer)u);
      else
        lua_pushnumber(L, (lua_Number)u);
    }
    else {
      lua_pushinteger(L, (lua_Integer)ffi_to_int(L, S, 1, NULL));
    }
    return 1;
  }
  {
    int n = lua_gettop(L);
    lua_getfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_tonumber");
    if (!lua_isfunction(L, -1))
      luaL_error(L, "ffi: original tonumber is not available");
    lua_insert(L, 1);
    lua_call(L, n, LUA_MULTRET);
    return lua_gettop(L);
  }
}


/*
** string.format("%d", cdata): Lua 5.5's formatter only accepts Lua
** numbers, while LuaJIT formats numeric cdata.  Rewrite just the numeric
** cdata arguments according to their conversion specifier, then delegate.
*/
static int ffi_lua_format (lua_State *L) {
  FFIState *S = ffi_getstate(L);
  int n = lua_gettop(L);
  size_t len = 0, i = 0;
  const char *fmt = NULL;
  int arg = 2;
  if (lua_type(L, 1) == LUA_TSTRING)
    fmt = lua_tolstring(L, 1, &len);
  else
    goto callorig;
  {
    int k, hascdata = 0;
    for (k = 2; k <= n; k++) {
      if (ffi_iscdata(L, k)) { hascdata = 1; break; }
    }
    if (!hascdata)
      goto callorig;
  }
  while (fmt != NULL && i < len) {
    char conv;
    if (fmt[i] != '%') { i++; continue; }
    i++;
    if (i < len && fmt[i] == '%') { i++; continue; }
    while (i < len && strchr("-+ #0", fmt[i]) != NULL) i++;
    while (i < len && isdigit((unsigned char)fmt[i])) i++;
    if (i < len && fmt[i] == '.') {
      i++;
      while (i < len && isdigit((unsigned char)fmt[i])) i++;
    }
    if (i >= len) break;
    conv = fmt[i++];
    if (arg <= n && ffi_iscdata(L, arg)) {
      CData *cd = ffi_tocdata(L, arg);
      if (cd != NULL) {
        /* LuaJIT formats cdata via tostring for %s and %q, including
        ** pointers, arrays, aggregates and ctype objects; stock 5.5
        ** already does that for %s but rejects %q.  Integer conversions
        ** still substitute a Lua integer for numeric cdata, and float
        ** cdata is not accepted by %d/%f. */
        if (conv == 's' || conv == 'q') {
          luaL_tolstring(L, arg, NULL);
          lua_replace(L, arg);
        }
        else if ((cd->flags & CDATA_CTYPE) == 0 &&
                 ffi_is_numeric(cd->ct) &&
                 strchr("diouxX", conv) != NULL &&
                 cd->ct->kind != CT_FLOAT) {
          int ok = 0;
          int64_t v = ffi_to_int(L, S, arg, &ok);
          if (ok) {
            lua_pushinteger(L, (lua_Integer)v);
            lua_replace(L, arg);
          }
        }
      }
    }
    arg++;
  }
callorig:
  lua_getfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_format");
  lua_insert(L, 1);
  lua_call(L, n, LUA_MULTRET);
  return lua_gettop(L);
}


static void install_type_wrapper (lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_type");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    lua_getglobal(L, "type");
    if (lua_isfunction(L, -1)) {
      lua_pushvalue(L, -1);
      lua_setfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_type");
      lua_pop(L, 1);
      lua_pushcfunction(L, ffi_lua_type);
      lua_setglobal(L, "type");
    }
    else {
      lua_pop(L, 1);
    }
  }
  else {
    lua_pop(L, 1);
  }
  lua_getfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_tonumber");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    lua_getglobal(L, "tonumber");
    if (lua_isfunction(L, -1)) {
      lua_pushvalue(L, -1);
      lua_setfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_tonumber");
      lua_pop(L, 1);
      lua_pushcfunction(L, ffi_lua_tonumber);
      lua_setglobal(L, "tonumber");
    }
    else {
      lua_pop(L, 1);
    }
  }
  else {
    lua_pop(L, 1);
  }
  lua_getfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_format");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    lua_getglobal(L, "string");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "format");
      if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "luar.ffi.orig_format");
        lua_pushcfunction(L, ffi_lua_format);
        lua_setfield(L, -3, "format");
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  else {
    lua_pop(L, 1);
  }
}


LUA_API int luar_has_ffi (void) {
  return 1;
}


LUALIB_API int luaopen_ffi (lua_State *L) {
  FFIState *S;
  int stateidx;
  int uv;
  luaL_checkversion(L);

  lua_getfield(L, LUA_REGISTRYINDEX, LUAR_FFI_REGISTRY);
  if (lua_type(L, -1) == LUA_TUSERDATA) {
    lua_getiuservalue(L, -1, FFI_UV_LIB);
    lua_remove(L, -2);
    return 1;
  }
  lua_pop(L, 1);

  ffi_libffi_init(L);		/* fail cleanly before allocating state */

  create_metatables(L);

  S = (FFIState *)lua_newuserdatauv(L, sizeof(FFIState), FFI_NUV);
  memset(S, 0, sizeof(*S));
  S->L = L;
  S->allocs = NULL;
  luaL_setmetatable(L, LUAR_FFI_STATE_MT);
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, LUAR_FFI_REGISTRY);
  for (uv = 1; uv <= FFI_NUV; uv++) {
    lua_newtable(L);
    lua_setiuservalue(L, -2, uv);
  }
  stateidx = lua_gettop(L);

  ffi_type_init_builtins(L, S);

  luaL_newlib(L, ffi_lib);
  lua_pushstring(L, luar_os());
  lua_setfield(L, -2, "os");
  lua_pushstring(L, luar_arch());
  lua_setfield(L, -2, "arch");

  ffi_push_default_clib(L, S);
  lua_setfield(L, -2, "C");

  /* LuaJIT's ffi.NULL is plain nil; ffi.cast("void *", 0) is the
  ** cdata form of a NULL pointer. */
  lua_pushnil(L);
  lua_setfield(L, -2, "NULL");

  lua_pushvalue(L, -1);
  lua_setiuservalue(L, stateidx, FFI_UV_LIB);

  install_type_wrapper(L);

  lua_remove(L, stateidx);
  return 1;
}
