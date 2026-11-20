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
  if (cd->ct->kind == CT_STRUCT || cd->ct->kind == CT_UNION)
    return 3;
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


int luar_ffi_trace_field (TValue *cdv, const char *name, int *kind,
                          size_t *off, size_t *size) {
  Udata *u;
  CData *cd;
  CType *ct;
  uint16_t i;
  if (cdv == NULL || name == NULL || kind == NULL || off == NULL ||
      size == NULL || !ttisfulluserdata(cdv))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if (ct->kind != CT_STRUCT && ct->kind != CT_UNION)
    return 0;
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    CType *ft = f->type;
    if (f->name == NULL || strcmp(f->name, name) != 0)
      continue;
    if (f->bitwidth != 0 || ft == NULL)
      return 0;
    if (ft->kind == CT_INT &&
        (ft->size == 1 || ft->size == 2 || ft->size == 4 || ft->size == 8))
      *kind = 1;
    else if (ft->kind == CT_FLOAT && (ft->size == 4 || ft->size == 8))
      *kind = 2;
    else
      return 0;
    *off = f->offset;
    *size = ft->size;
    return 1;
  }
  return 0;
}


int luar_ffi_trace_field2 (TValue *cdv, const char *outer, const char *inner,
                           int *kind, size_t *off, size_t *size) {
  Udata *u;
  CData *cd;
  CType *ct, *oct = NULL;
  uint16_t i, j;
  size_t ooff = 0;
  if (cdv == NULL || outer == NULL || inner == NULL || kind == NULL ||
      off == NULL || size == NULL || !ttisfulluserdata(cdv))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if (ct->kind != CT_STRUCT && ct->kind != CT_UNION)
    return 0;
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    if (f->name != NULL && strcmp(f->name, outer) == 0 &&
        f->bitwidth == 0 && f->type != NULL &&
        (f->type->kind == CT_STRUCT || f->type->kind == CT_UNION)) {
      oct = f->type;
      ooff = f->offset;
      break;
    }
  }
  if (oct == NULL)
    return 0;
  for (j = 0; j < oct->nfields; j++) {
    CField *f = &oct->fields[j];
    CType *ft = f->type;
    if (f->name == NULL || strcmp(f->name, inner) != 0)
      continue;
    if (f->bitwidth != 0 || ft == NULL)
      return 0;
    if (ft->kind == CT_INT &&
        (ft->size == 1 || ft->size == 2 || ft->size == 4 || ft->size == 8))
      *kind = 1;
    else if (ft->kind == CT_FLOAT && (ft->size == 4 || ft->size == 8))
      *kind = 2;
    else
      return 0;
    *off = ooff + f->offset;
    *size = ft->size;
    return 1;
  }
  return 0;
}


int64_t luar_ffi_trace_cget_field_int (TValue *cdv, size_t off, size_t sz,
                                       TValue *out) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *p;
  int64_t v = 0;
  if (cdv == NULL || out == NULL || !ttisfulluserdata(cdv) ||
      (sz != 1 && sz != 2 && sz != 4 && sz != 8))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size || sz > ct->size - off)
    return 0;
  p = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
      cd->v.p : (void *)&cd->v;
  p = (char *)p + off;
  if (sz == 1) { int8_t x; memcpy(&x, p, 1); v = x; }
  else if (sz == 2) { int16_t x; memcpy(&x, p, 2); v = x; }
  else if (sz == 4) { int32_t x; memcpy(&x, p, 4); v = x; }
  else memcpy(&v, p, 8);
  setivalue(out, (lua_Integer)v);
  return 1;
}


int64_t luar_ffi_trace_cget_field_double (TValue *cdv, size_t off, size_t sz,
                                          TValue *out) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *p;
  double d;
  if (cdv == NULL || out == NULL || !ttisfulluserdata(cdv) ||
      (sz != 4 && sz != 8))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size || sz > ct->size - off)
    return 0;
  p = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
      cd->v.p : (void *)&cd->v;
  p = (char *)p + off;
  if (sz == 4) { float f; memcpy(&f, p, 4); d = (double)f; }
  else memcpy(&d, p, 8);
  setfltvalue(out, d);
  return 1;
}


int64_t luar_ffi_trace_cset_field_int (TValue *cdv, size_t off, size_t sz,
                                       TValue *val) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *p;
  int64_t v;
  if (cdv == NULL || val == NULL || !ttisfulluserdata(cdv) ||
      !ttisinteger(val) || (sz != 1 && sz != 2 && sz != 4 && sz != 8))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size || sz > ct->size - off)
    return 0;
  v = (int64_t)ivalue(val);
  p = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
      cd->v.p : (void *)&cd->v;
  p = (char *)p + off;
  if (sz == 1) { uint8_t x = (uint8_t)v; memcpy(p, &x, 1); }
  else if (sz == 2) { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); }
  else if (sz == 4) { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); }
  else { uint64_t x = (uint64_t)v; memcpy(p, &x, 8); }
  return 1;
}


int64_t luar_ffi_trace_cset_field_double (TValue *cdv, size_t off, size_t sz,
                                          TValue *val) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *p;
  double d;
  if (cdv == NULL || val == NULL || !ttisfulluserdata(cdv) ||
      !ttisfloat(val) || (sz != 4 && sz != 8))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size || sz > ct->size - off)
    return 0;
  d = fltvalue(val);
  p = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
      cd->v.p : (void *)&cd->v;
  p = (char *)p + off;
  if (sz == 4) { float f = (float)d; memcpy(p, &f, 4); }
  else memcpy(p, &d, 8);
  return 1;
}


int64_t luar_ffi_trace_cset_field_int_const (TValue *cdv, size_t off,
                                             size_t sz, int64_t v) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *p;
  if (cdv == NULL || !ttisfulluserdata(cdv) ||
      (sz != 1 && sz != 2 && sz != 4 && sz != 8))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size || sz > ct->size - off)
    return 0;
  p = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
      cd->v.p : (void *)&cd->v;
  p = (char *)p + off;
  if (sz == 1) { uint8_t x = (uint8_t)v; memcpy(p, &x, 1); }
  else if (sz == 2) { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); }
  else if (sz == 4) { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); }
  else { uint64_t x = (uint64_t)v; memcpy(p, &x, 8); }
  return 1;
}


int64_t luar_ffi_trace_cset_field_double_const (TValue *cdv, size_t off,
                                                size_t sz, double d) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *p;
  if (cdv == NULL || !ttisfulluserdata(cdv) || (sz != 4 && sz != 8))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size || sz > ct->size - off)
    return 0;
  p = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
      cd->v.p : (void *)&cd->v;
  p = (char *)p + off;
  if (sz == 4) { float f = (float)d; memcpy(p, &f, 4); }
  else memcpy(p, &d, 8);
  return 1;
}
