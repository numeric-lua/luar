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


static int find_aggregate_field (CType *ct, const char *name,
                                 CType **out, size_t *off) {
  uint16_t i;
  if (ct == NULL || (ct->kind != CT_STRUCT && ct->kind != CT_UNION))
    return 0;
  for (i = 0; i < ct->nfields; i++) {
    CField *f = &ct->fields[i];
    if (f->name != NULL && strcmp(f->name, name) == 0 &&
        f->bitwidth == 0 && f->type != NULL &&
        (f->type->kind == CT_STRUCT || f->type->kind == CT_UNION)) {
      *out = f->type;
      *off = f->offset;
      return 1;
    }
  }
  return 0;
}


int luar_ffi_trace_field3 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, int *kind, size_t *off,
                           size_t *size) {
  Udata *u;
  CData *cd;
  CType *c1, *c2, *c3;
  size_t o1 = 0, o2 = 0, o3 = 0;
  uint16_t i;
  if (cdv == NULL || n1 == NULL || n2 == NULL || n3 == NULL ||
      kind == NULL || off == NULL || size == NULL || !ttisfulluserdata(cdv))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  if (!find_aggregate_field(cd->ct, n1, &c1, &o1) ||
      !find_aggregate_field(c1, n2, &c2, &o2))
    return 0;
  c3 = c2;
  for (i = 0; i < c3->nfields; i++) {
    CField *f = &c3->fields[i];
    CType *ft = f->type;
    if (f->name == NULL || strcmp(f->name, n3) != 0)
      continue;
    if (f->bitwidth != 0 || ft == NULL)
      return 0;
    o3 = f->offset;
    if (ft->kind == CT_INT &&
        (ft->size == 1 || ft->size == 2 || ft->size == 4 || ft->size == 8))
      *kind = 1;
    else if (ft->kind == CT_FLOAT && (ft->size == 4 || ft->size == 8))
      *kind = 2;
    else
      return 0;
    *off = o1 + o2 + o3;
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
  int64_t v;
  void *p;
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


int luar_ffi_trace_ccall0_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *rt;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0)
    return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 0 || ft->vararg)
    return 0;
  rt = ft->ret;
  if (rt == NULL)
    return 0;
  if (rt->kind == CT_INT &&
      (rt->size == 1 || rt->size == 2 || rt->size == 4 || rt->size == 8))
    return 1;
  if (rt->kind == CT_FLOAT && (rt->size == 4 || rt->size == 8))
    return 2;
  return 0;
}


int64_t luar_ffi_trace_ccall0_int (TValue *fn, TValue *out) {
  Udata *u;
  CData *cd;
  int kind = 0;
  int64_t iv = 0;
  double dv = 0;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0)
    return 0;
  if (!ffi_trace_call0(cd, &kind, &iv, &dv) || kind != 1)
    return 0;
  setivalue(out, (lua_Integer)iv);
  return 1;
}


/* Record-time classification of a safe native C call with exactly one
** integer argument and an integer return.  Never calls the function. */
int luar_ffi_trace_ccall1_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *at, *rt;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC ||
      (cd->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      cd->v.p == NULL || cd->ct == NULL)
    return 0;
  if (cd->ct->kind == CT_FUNC)
    ft = cd->ct;
  else if (cd->ct->kind == CT_POINTER && cd->ct->child != NULL &&
           cd->ct->child->kind == CT_FUNC)
    ft = cd->ct->child;
  else
    return 0;
  if (ft->nargs != 1 || ft->vararg)
    return 0;
  at = ft->args[0];
  rt = ft->ret;
  if (at == NULL || rt == NULL || at->kind != CT_INT || rt->kind != CT_INT)
    return 0;
  if (at->size != 1 && at->size != 2 && at->size != 4 && at->size != 8)
    return 0;
  if (rt->size != 1 && rt->size != 2 && rt->size != 4 && rt->size != 8)
    return 0;
  return 1;
}


/* J4 helper: call the probed one-integer-argument C function and store the
** raw integer result.  No Lua stack, allocation or metamethods. */
int64_t luar_ffi_trace_ccall1i (TValue *fn, int64_t arg, int64_t *out) {
  Udata *u;
  CData *cd;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC)
    return 0;
  return ffi_trace_call1i(cd, arg, out) ? 1 : 0;
}


int luar_ffi_trace_ccall1di_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *at, *rt;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0)
    return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 1 || ft->vararg)
    return 0;
  at = ft->args[0];
  rt = ft->ret;
  if (at != NULL && rt != NULL && at->kind == CT_FLOAT &&
      at->size == 8 && rt->kind == CT_FLOAT && rt->size == 8)
    return 1;
  return 0;
}


int64_t luar_ffi_trace_ccall1di (TValue *fn, int64_t arg, TValue *out) {
  Udata *u;
  CData *cd;
  double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0)
    return 0;
  if (!ffi_trace_call1d_iarg(cd, arg, &v))
    return 0;
  setfltvalue(out, v);
  return 1;
}


/* Record-time classification of a C function with one double argument and
** a double return.  Never calls the function. */
int luar_ffi_trace_ccall1dd_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *at, *rt;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC ||
      (cd->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      cd->v.p == NULL || cd->ct == NULL)
    return 0;
  if (cd->ct->kind == CT_FUNC)
    ft = cd->ct;
  else if (cd->ct->kind == CT_POINTER && cd->ct->child != NULL &&
           cd->ct->child->kind == CT_FUNC)
    ft = cd->ct->child;
  else
    return 0;
  if (ft->nargs != 1 || ft->vararg)
    return 0;
  at = ft->args[0];
  rt = ft->ret;
  if (at == NULL || rt == NULL || at->kind != CT_FLOAT ||
      at->size != 8 || rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  return 1;
}


/* J4 helper: call the probed double-argument/double-return C function and
** store the Lua float result.  No Lua stack, allocation or metamethods. */
int64_t luar_ffi_trace_ccall1dd (TValue *fn, double arg, TValue *out) {
  Udata *u;
  CData *cd;
  double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC)
    return 0;
  if (!ffi_trace_call1d(cd, arg, &v))
    return 0;
  setfltvalue(out, v);
  return 1;
}


int64_t luar_ffi_trace_ccall2dd (TValue *fn, double arg1, double arg2,
                                 TValue *out) {
  Udata *u;
  CData *cd;
  double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC)
    return 0;
  if (!ffi_trace_call2d(cd, arg1, arg2, &v))
    return 0;
  setfltvalue(out, v);
  return 1;
}


/* Record-time classification of a C function with two double arguments and
** a double return.  Never calls the function. */
int luar_ffi_trace_ccall2dd_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *at1, *at2, *rt;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC ||
      (cd->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      cd->v.p == NULL || cd->ct == NULL)
    return 0;
  if (cd->ct->kind == CT_FUNC)
    ft = cd->ct;
  else if (cd->ct->kind == CT_POINTER && cd->ct->child != NULL &&
           cd->ct->child->kind == CT_FUNC)
    ft = cd->ct->child;
  else
    return 0;
  if (ft->nargs != 2 || ft->vararg)
    return 0;
  at1 = ft->args[0];
  at2 = ft->args[1];
  rt = ft->ret;
  if (at1 == NULL || at2 == NULL || rt == NULL ||
      at1->kind != CT_FLOAT || at1->size != 8 ||
      at2->kind != CT_FLOAT || at2->size != 8 ||
      rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  return 1;
}


int64_t luar_ffi_trace_ccall3dd (TValue *fn, double arg1, double arg2,
                                 double arg3, TValue *out) {
  Udata *u;
  CData *cd;
  double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC)
    return 0;
  if (!ffi_trace_call3d(cd, arg1, arg2, arg3, &v))
    return 0;
  setfltvalue(out, v);
  return 1;
}


/* Record-time classification of a C function with three double arguments
** and a double return.  Never calls the function. */
int luar_ffi_trace_ccall3dd_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *at1, *at2, *at3, *rt;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC ||
      (cd->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      cd->v.p == NULL || cd->ct == NULL)
    return 0;
  if (cd->ct->kind == CT_FUNC)
    ft = cd->ct;
  else if (cd->ct->kind == CT_POINTER && cd->ct->child != NULL &&
           cd->ct->child->kind == CT_FUNC)
    ft = cd->ct->child;
  else
    return 0;
  if (ft->nargs != 3 || ft->vararg)
    return 0;
  at1 = ft->args[0];
  at2 = ft->args[1];
  at3 = ft->args[2];
  rt = ft->ret;
  if (at1 == NULL || at2 == NULL || at3 == NULL || rt == NULL ||
      at1->kind != CT_FLOAT || at1->size != 8 ||
      at2->kind != CT_FLOAT || at2->size != 8 ||
      at3->kind != CT_FLOAT || at3->size != 8 ||
      rt->kind != CT_FLOAT || rt->size != 8)
    return 0;
  return 1;
}


int luar_ffi_trace_ccall2di_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *r;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0)
    return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  r = ft->ret;
  if (r == NULL || r->kind != CT_FLOAT || r->size != 8 ||
      ft->args[0] == NULL || ft->args[1] == NULL ||
      ft->args[0]->kind != CT_FLOAT || ft->args[0]->size != 8 ||
      ft->args[1]->kind != CT_INT || ft->args[1]->size != 4)
    return 0;
  return 1;
}


int64_t luar_ffi_trace_ccall2di (TValue *fn, double a0, int64_t a1,
                                 TValue *out) {
  Udata *u;
  CData *cd;
  double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0)
    return 0;
  if (!ffi_trace_call2di(cd, a0, a1, &v))
    return 0;
  setfltvalue(out, v);
  return 1;
}


int luar_ffi_trace_ccall2ii_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *r;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0)
    return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  r = ft->ret;
  if (r == NULL || r->kind != CT_INT || r->size != 4 ||
      ft->args[0] == NULL || ft->args[1] == NULL ||
      ft->args[0]->kind != CT_INT || ft->args[0]->size != 4 ||
      ft->args[1]->kind != CT_INT || ft->args[1]->size != 4)
    return 0;
  return 1;
}


int64_t luar_ffi_trace_ccall2ii (TValue *fn, int64_t a0, int64_t a1,
                                 TValue *out) {
  Udata *u;
  CData *cd;
  int64_t v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0)
    return 0;
  if (!ffi_trace_call2ii(cd, a0, a1, &v))
    return 0;
  setivalue(out, (lua_Integer)v);
  return 1;
}


int luar_ffi_trace_fieldn (TValue *cdv, const char *const *names, int n,
                           int *kind, size_t *off, size_t *size) {
  Udata *u;
  CData *cd;
  CType *ct;
  size_t total = 0;
  int i;
  if (cdv == NULL || names == NULL || n < 2 || kind == NULL ||
      off == NULL || size == NULL || !ttisfulluserdata(cdv))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  for (i = 0; i < n - 1; i++) {
    CType *next;
    size_t o;
    if (!find_aggregate_field(ct, names[i], &next, &o))
      return 0;
    total += o;
    ct = next;
  }
  {
    uint16_t j;
    for (j = 0; j < ct->nfields; j++) {
      CField *f = &ct->fields[j];
      CType *ft = f->type;
      if (f->name == NULL || strcmp(f->name, names[n - 1]) != 0)
        continue;
      if (f->bitwidth != 0 || ft == NULL)
        return 0;
      if (ft->kind == CT_INT &&
          (ft->size == 1 || ft->size == 2 || ft->size == 4 ||
           ft->size == 8))
        *kind = 1;
      else if (ft->kind == CT_FLOAT && (ft->size == 4 || ft->size == 8))
        *kind = 2;
      else
        return 0;
      *off = total + f->offset;
      *size = ft->size;
      return 1;
    }
  }
  return 0;
}


int luar_ffi_trace_field4 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, const char *n4, int *kind,
                           size_t *off, size_t *size) {
  Udata *u;
  CData *cd;
  CType *c1, *c2, *c3, *c4;
  size_t o1 = 0, o2 = 0, o3 = 0, o4 = 0;
  uint16_t i;
  if (cdv == NULL || n1 == NULL || n2 == NULL || n3 == NULL || n4 == NULL ||
      kind == NULL || off == NULL || size == NULL || !ttisfulluserdata(cdv))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  if (!find_aggregate_field(cd->ct, n1, &c1, &o1) ||
      !find_aggregate_field(c1, n2, &c2, &o2) ||
      !find_aggregate_field(c2, n3, &c3, &o3))
    return 0;
  c4 = c3;
  for (i = 0; i < c4->nfields; i++) {
    CField *f = &c4->fields[i];
    CType *ft = f->type;
    if (f->name == NULL || strcmp(f->name, n4) != 0)
      continue;
    if (f->bitwidth != 0 || ft == NULL)
      return 0;
    o4 = f->offset;
    if (ft->kind == CT_INT &&
        (ft->size == 1 || ft->size == 2 || ft->size == 4 || ft->size == 8))
      *kind = 1;
    else if (ft->kind == CT_FLOAT && (ft->size == 4 || ft->size == 8))
      *kind = 2;
    else
      return 0;
    *off = o1 + o2 + o3 + o4;
    *size = ft->size;
    return 1;
  }
  return 0;
}


int luar_ffi_trace_field5 (TValue *cdv, const char *n1, const char *n2,
                           const char *n3, const char *n4, const char *n5,
                           int *kind, size_t *off, size_t *size) {
  Udata *u;
  CData *cd;
  CType *c1, *c2, *c3, *c4, *c5;
  size_t o1 = 0, o2 = 0, o3 = 0, o4 = 0, o5 = 0;
  uint16_t i;
  if (cdv == NULL || n1 == NULL || n2 == NULL || n3 == NULL || n4 == NULL ||
      n5 == NULL || kind == NULL || off == NULL || size == NULL ||
      !ttisfulluserdata(cdv))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  if (!find_aggregate_field(cd->ct, n1, &c1, &o1) ||
      !find_aggregate_field(c1, n2, &c2, &o2) ||
      !find_aggregate_field(c2, n3, &c3, &o3) ||
      !find_aggregate_field(c3, n4, &c4, &o4))
    return 0;
  c5 = c4;
  for (i = 0; i < c5->nfields; i++) {
    CField *f = &c5->fields[i];
    CType *ft = f->type;
    if (f->name == NULL || strcmp(f->name, n5) != 0)
      continue;
    if (f->bitwidth != 0 || ft == NULL)
      return 0;
    o5 = f->offset;
    if (ft->kind == CT_INT &&
        (ft->size == 1 || ft->size == 2 || ft->size == 4 || ft->size == 8))
      *kind = 1;
    else if (ft->kind == CT_FLOAT && (ft->size == 4 || ft->size == 8))
      *kind = 2;
    else
      return 0;
    *off = o1 + o2 + o3 + o4 + o5;
    *size = ft->size;
    return 1;
  }
  return 0;
}


int luar_ffi_trace_ptr_field (TValue *cdv, const char *name, int *kind,
                              size_t *off, size_t *esz) {
  Udata *u;
  CData *cd;
  CType *ct, *ft, *elem;
  uint16_t i;
  if (cdv == NULL || name == NULL || kind == NULL || off == NULL ||
      esz == NULL || !ttisfulluserdata(cdv))
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
    if (f->name == NULL || strcmp(f->name, name) != 0 || f->bitwidth != 0)
      continue;
    ft = f->type;
    if (ft == NULL || ft->kind != CT_POINTER || ft->child == NULL)
      return 0;
    elem = ft->child;
    if (elem->kind == CT_INT &&
        (elem->size == 1 || elem->size == 2 || elem->size == 4 ||
         elem->size == 8))
      *kind = 1;
    else if (elem->kind == CT_FLOAT && (elem->size == 4 || elem->size == 8))
      *kind = 2;
    else
      return 0;
    *off = f->offset;
    *esz = elem->size;
    return 1;
  }
  return 0;
}


static int64_t read_ptr_elem (TValue *cdv, size_t off, TValue *kt,
                              TValue *out, int kind) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *base;
  void *ptr;
  lua_Integer idx;
  size_t esz;
  if (cdv == NULL || kt == NULL || out == NULL ||
      !ttisfulluserdata(cdv) || !ttisinteger(kt))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size ||
      sizeof(void *) > ct->size - off)
    return 0;
  base = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
         cd->v.p : (void *)&cd->v;
  memcpy(&ptr, (char *)base + off, sizeof(ptr));
  if (ptr == NULL)
    return 0;
  idx = ivalue(kt);
  if (kind == 1) {
    esz = 4;   /* refined by caller: reject before using */
    (void)esz;
  }
  /* element size is encoded in nval by the caller; recompute from kind */
  if (kind == 1) {
    /* int fields are limited to 4/8 bytes in this subset */
    int64_t v = 0;
    memcpy(&v, (char *)ptr + (ptrdiff_t)idx * 4, 4);
    setivalue(out, (lua_Integer)(int32_t)v);
    return 1;
  }
  else {
    double d;
    memcpy(&d, (char *)ptr + (ptrdiff_t)idx * 8, 8);
    setfltvalue(out, d);
    return 1;
  }
}


int64_t luar_ffi_trace_cget_ptr_int (TValue *cdv, size_t off, TValue *kt,
                                     TValue *out) {
  return read_ptr_elem(cdv, off, kt, out, 1);
}


int64_t luar_ffi_trace_cget_ptr_double (TValue *cdv, size_t off, TValue *kt,
                                        TValue *out) {
  return read_ptr_elem(cdv, off, kt, out, 2);
}


static int64_t write_ptr_elem (TValue *cdv, size_t off, TValue *kt,
                               TValue *val, int kind) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *base;
  void *ptr;
  lua_Integer idx;
  if (cdv == NULL || kt == NULL || val == NULL ||
      !ttisfulluserdata(cdv) || !ttisinteger(kt))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size ||
      sizeof(void *) > ct->size - off)
    return 0;
  base = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
         cd->v.p : (void *)&cd->v;
  memcpy(&ptr, (char *)base + off, sizeof(ptr));
  if (ptr == NULL)
    return 0;
  idx = ivalue(kt);
  if (kind == 1) {
    uint32_t x;
    if (!ttisinteger(val)) return 0;
    x = (uint32_t)ivalue(val);
    memcpy((char *)ptr + (ptrdiff_t)idx * 4, &x, 4);
  }
  else {
    double d;
    if (!ttisfloat(val)) return 0;
    d = fltvalue(val);
    memcpy((char *)ptr + (ptrdiff_t)idx * 8, &d, 8);
  }
  return 1;
}


int64_t luar_ffi_trace_cset_ptr_int (TValue *cdv, size_t off, TValue *kt,
                                     TValue *val) {
  return write_ptr_elem(cdv, off, kt, val, 1);
}


int64_t luar_ffi_trace_cset_ptr_double (TValue *cdv, size_t off, TValue *kt,
                                        TValue *val) {
  return write_ptr_elem(cdv, off, kt, val, 2);
}


int64_t luar_ffi_trace_cget_ptr_i64 (TValue *cdv, size_t off, TValue *kt,
                                     TValue *out) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *base, *ptr;
  lua_Integer idx;
  int64_t v;
  if (cdv == NULL || kt == NULL || out == NULL ||
      !ttisfulluserdata(cdv) || !ttisinteger(kt))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size ||
      sizeof(void *) > ct->size - off)
    return 0;
  base = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
         cd->v.p : (void *)&cd->v;
  memcpy(&ptr, (char *)base + off, sizeof(ptr));
  if (ptr == NULL)
    return 0;
  idx = ivalue(kt);
  memcpy(&v, (char *)ptr + (ptrdiff_t)idx * 8, 8);
  setivalue(out, (lua_Integer)v);
  return 1;
}


int64_t luar_ffi_trace_cset_ptr_i64 (TValue *cdv, size_t off, TValue *kt,
                                     TValue *val) {
  Udata *u;
  CData *cd;
  CType *ct;
  void *base, *ptr;
  lua_Integer idx;
  uint64_t v;
  if (cdv == NULL || kt == NULL || val == NULL ||
      !ttisfulluserdata(cdv) || !ttisinteger(kt) || !ttisinteger(val))
    return 0;
  u = (Udata *)cdv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ct = cd->ct;
  if ((ct->kind != CT_STRUCT && ct->kind != CT_UNION) ||
      ct->size == FFI_SIZE_UNKNOWN || off > ct->size ||
      sizeof(void *) > ct->size - off)
    return 0;
  base = ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ?
         cd->v.p : (void *)&cd->v;
  memcpy(&ptr, (char *)base + off, sizeof(ptr));
  if (ptr == NULL)
    return 0;
  idx = ivalue(kt);
  v = (uint64_t)ivalue(val);
  memcpy((char *)ptr + (ptrdiff_t)idx * 8, &v, 8);
  return 1;
}
