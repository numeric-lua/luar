/*
** luar_ffi_trace.c -- small, allocation-free FFI subset for J4 traces.
**
** Only cdata integer arrays/pointers with a 1/2/4/8-byte integer element
** are exposed here.  The helpers validate types and bounds and never call
** Lua, allocate, or invoke metamethods.
*/
#include "lprefix.h"

#include <stdint.h>
#include <string.h>

#include "lua.h"
#include "lobject.h"
#include "lstate.h"

#include "luar_ffi_internal.h"

int luar_ffi_trace_kind (TValue *tv) {
  Udata *u;
  CData *cd;
  if (tv == NULL || !ttisfulluserdata(tv))
    return -1;
  u = (Udata *)tv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return -1;
  if (cd->ct->kind == CT_ARRAY || cd->ct->kind == CT_POINTER) {
    CType *elem = cd->ct->child;
    if (elem != NULL && elem->kind == CT_INT &&
        (elem->size == 1 || elem->size == 2 ||
         elem->size == 4 || elem->size == 8))
      return 1;
    if (elem != NULL && elem->kind == CT_FLOAT &&
        (elem->size == 4 || elem->size == 8))
      return 2;
  }
  return -1;
}


int64_t luar_ffi_trace_cget_int (TValue *cdv, TValue *kt, TValue *out) {
  Udata *u;
  CData *cd;
  CType *ct, *elem;
  void *base, *p;
  size_t len = 0, esz;
  lua_Integer idx;
  int64_t v = 0;
  if (cdv == NULL || kt == NULL || out == NULL ||
      !ttisfulluserdata(cdv) || !ttisinteger(kt))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if (ct->kind == CT_ARRAY) {
    elem = ct->child;
    base = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
           cd->v.p : (void *)&cd->v;
    len = ct->vla ? cd->len : ct->length;
  }
  else if (ct->kind == CT_POINTER) {
    elem = ct->child;
    base = cd->v.p;
    if (base == NULL)
      return 0;
  }
  else
    return 0;
  if (elem == NULL || elem->kind != CT_INT ||
      (elem->size != 1 && elem->size != 2 &&
       elem->size != 4 && elem->size != 8))
    return 0;
  idx = ivalue(kt);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  esz = elem->size;
  p = (char *)base + (ptrdiff_t)idx * (ptrdiff_t)esz;
  if (esz == 1) { int8_t x; memcpy(&x, p, 1); v = x; }
  else if (esz == 2) { int16_t x; memcpy(&x, p, 2); v = x; }
  else if (esz == 4) { int32_t x; memcpy(&x, p, 4); v = x; }
  else { memcpy(&v, p, 8); }
  if (elem->is_unsigned) {
    switch (esz) {
      case 1: v = (uint8_t)v; break;
      case 2: v = (uint16_t)v; break;
      case 4: v = (uint32_t)v; break;
      default: break;
    }
  }
  setivalue(out, (lua_Integer)v);
  return 1;
}


int64_t luar_ffi_trace_cget_double (TValue *cdv, TValue *kt, TValue *out) {
  Udata *u;
  CData *cd;
  CType *ct, *elem;
  void *base, *p;
  size_t len = 0, esz;
  lua_Integer idx;
  double d;
  if (cdv == NULL || kt == NULL || out == NULL ||
      !ttisfulluserdata(cdv) || !ttisinteger(kt))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if (ct->kind == CT_ARRAY) {
    elem = ct->child;
    base = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
           cd->v.p : (void *)&cd->v;
    len = ct->vla ? cd->len : ct->length;
  }
  else if (ct->kind == CT_POINTER) {
    elem = ct->child;
    base = cd->v.p;
    if (base == NULL)
      return 0;
  }
  else
    return 0;
  if (elem == NULL || elem->kind != CT_FLOAT ||
      (elem->size != 4 && elem->size != 8))
    return 0;
  idx = ivalue(kt);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  esz = elem->size;
  p = (char *)base + (ptrdiff_t)idx * (ptrdiff_t)esz;
  if (esz == 4) { float f; memcpy(&f, p, 4); d = (double)f; }
  else memcpy(&d, p, 8);
  setfltvalue(out, d);
  return 1;
}


int64_t luar_ffi_trace_cset_int (TValue *cdv, TValue *kt, TValue *val) {
  Udata *u;
  CData *cd;
  CType *ct, *elem;
  void *base, *p;
  size_t len = 0, esz;
  lua_Integer idx, v;
  if (cdv == NULL || kt == NULL || val == NULL ||
      !ttisfulluserdata(cdv) || !ttisinteger(kt) || !ttisinteger(val))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if (ct->kind == CT_ARRAY) {
    elem = ct->child;
    base = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
           cd->v.p : (void *)&cd->v;
    len = ct->vla ? cd->len : ct->length;
  }
  else if (ct->kind == CT_POINTER) {
    elem = ct->child;
    base = cd->v.p;
    if (base == NULL)
      return 0;
  }
  else
    return 0;
  if (elem == NULL || elem->kind != CT_INT ||
      (elem->size != 1 && elem->size != 2 &&
       elem->size != 4 && elem->size != 8))
    return 0;
  idx = ivalue(kt);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  esz = elem->size;
  v = ivalue(val);
  p = (char *)base + (ptrdiff_t)idx * (ptrdiff_t)esz;
  if (esz == 1) { uint8_t x = (uint8_t)v; memcpy(p, &x, 1); }
  else if (esz == 2) { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); }
  else if (esz == 4) { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); }
  else { uint64_t x = (uint64_t)v; memcpy(p, &x, 8); }
  return 1;
}


int64_t luar_ffi_trace_cset_double (TValue *cdv, TValue *kt, TValue *val) {
  Udata *u;
  CData *cd;
  CType *ct, *elem;
  void *base, *p;
  size_t len = 0, esz;
  lua_Integer idx;
  double d;
  if (cdv == NULL || kt == NULL || val == NULL ||
      !ttisfulluserdata(cdv) || !ttisinteger(kt) || !ttisfloat(val))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if (ct->kind == CT_ARRAY) {
    elem = ct->child;
    base = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
           cd->v.p : (void *)&cd->v;
    len = ct->vla ? cd->len : ct->length;
  }
  else if (ct->kind == CT_POINTER) {
    elem = ct->child;
    base = cd->v.p;
    if (base == NULL)
      return 0;
  }
  else
    return 0;
  if (elem == NULL || elem->kind != CT_FLOAT ||
      (elem->size != 4 && elem->size != 8))
    return 0;
  idx = ivalue(kt);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  esz = elem->size;
  d = fltvalue(val);
  p = (char *)base + (ptrdiff_t)idx * (ptrdiff_t)esz;
  if (esz == 4) { float f = (float)d; memcpy(p, &f, 4); }
  else memcpy(p, &d, 8);
  return 1;
}
