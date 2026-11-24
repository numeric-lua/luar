/*
** luar_ffi_trace.c -- small, allocation-free FFI subset for J4 traces.
**
** Only cdata integer arrays/pointers with a 1/2/4/8-byte integer element
** are exposed here.  The helpers validate types and bounds and never call
** Lua, allocate, or invoke metamethods.
*/
#include "lprefix.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "lua.h"
#include "lobject.h"
#include "lstate.h"
#include "lvm.h"

#include "luar_ffi_internal.h"


/* Constant-size loads keep memcpy inlined; the generic ffi_load_uint takes
** a runtime size and turns every element access into a libc memcpy call,
** which made the J2 integer reduction an order of magnitude slower than
** the double one. */
static inline uint64_t trace_load_uint (const void *p, size_t sz) {
  switch (sz) {
    case 1: { uint8_t v; memcpy(&v, p, sizeof(v)); return v; }
    case 2: { uint16_t v; memcpy(&v, p, sizeof(v)); return v; }
    case 4: { uint32_t v; memcpy(&v, p, sizeof(v)); return v; }
    default: { uint64_t v; memcpy(&v, p, sizeof(v)); return v; }
  }
}


static inline int64_t trace_load_int (const void *p, size_t sz) {
  uint64_t u = trace_load_uint(p, sz);
  switch (sz) {
    case 1: return (int64_t)(int8_t)u;
    case 2: return (int64_t)(int16_t)u;
    case 4: return (int64_t)(int32_t)u;
    default: return (int64_t)u;
  }
}

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
    /* 8-byte cdata reads produce cdata int64 values, not Lua integers;
    ** the 32-bit CGETI path must not consume them. */
    if (elem != NULL && elem->kind == CT_INT &&
        (elem->size == 1 || elem->size == 2 || elem->size == 4))
      return 1;
    if (elem != NULL && elem->kind == CT_FLOAT &&
        (elem->size == 4 || elem->size == 8))
      return 2;
  }
  return -1;
}


/* Bounded cdata array variant used by the default J2 reduction.  A pointer
** has no length, so the intrinsic cannot prove the run is in bounds and
** must leave it to the interpreter. */
int luar_ffi_trace_array_kind (TValue *tv) {
  Udata *u;
  CData *cd;
  CType *elem;
  if (tv == NULL || !ttisfulluserdata(tv))
    return 0;
  u = (Udata *)tv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0 ||
      cd->ct == NULL || cd->ct->kind != CT_ARRAY || cd->ct->child == NULL)
    return 0;
  elem = cd->ct->child;
  if (elem->kind == CT_INT &&
      (elem->size == 1 || elem->size == 2 || elem->size == 4))
    return 1;
  /* An enum element reads as its unsigned 32-bit numeric view, exactly
  ** like ffi_push_value's CT_ENUM case. */
  if (elem->kind == CT_ENUM && elem->size == 4)
    return 1;
  if (elem->kind == CT_FLOAT &&
      (elem->size == 4 || elem->size == 8))
    return 2;
  return 0;
}


int luar_ffi_trace_array_kind64 (TValue *tv) {
  Udata *u;
  CData *cd;
  CType *elem;
  if (tv == NULL || !ttisfulluserdata(tv))
    return 0;
  u = (Udata *)tv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0 ||
      cd->ct == NULL || cd->ct->kind != CT_ARRAY || cd->ct->child == NULL)
    return 0;
  elem = cd->ct->child;
  return (elem->kind == CT_INT && elem->size == 8) ? 1 : 0;
}


/* Store-side element kind.  Unlike array_kind this admits 8-byte integer
** elements, because a bounded fill writes conversions of Lua numbers and
** never loads an element into a Lua slot.  Copy/update/pair helpers keep
** using array_kind and therefore stay away from cdata int64 temporaries. */
int luar_ffi_trace_array_store_kind (TValue *tv) {
  Udata *u;
  CData *cd;
  CType *elem;
  if (tv == NULL || !ttisfulluserdata(tv))
    return 0;
  u = (Udata *)tv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0 ||
      cd->ct == NULL || cd->ct->kind != CT_ARRAY || cd->ct->child == NULL)
    return 0;
  elem = cd->ct->child;
  if (elem->kind == CT_INT &&
      (elem->size == 1 || elem->size == 2 || elem->size == 4 ||
       elem->size == 8))
    return 1;
  /* Enum storage is a 32-bit integer store; ffi_convert uses the same
  ** unsigned conversion for CT_ENUM destinations. */
  if (elem->kind == CT_ENUM && elem->size == 4)
    return 1;
  /* A bool destination only stores the normalized truth of a Lua number;
  ** no element is ever loaded into a Lua slot. */
  if (elem->kind == CT_BOOL && elem->size == 1)
    return 3;
  if (elem->kind == CT_FLOAT &&
      (elem->size == 4 || elem->size == 8))
    return 2;
  return 0;
}


/* J2 cdata reduction: s = s + a[i] for a bounded array of 1/2/4-byte
** integers or 4/8-byte floats.  The loop control is exactly the interpreter
** FORLOOP state at the backedge: 'count + 1' body iterations start at
** 'idx', and on success the control slots are finished (count 0, index at
** the last visited key) so the VM resumes after FORLOOP. */
int luar_ffi_trace_sum_array (TValue *arrv, TValue *it, TValue *ct,
                              TValue *st, TValue *at, TValue *rt,
                              int sub) {
  Udata *u; CData *cd; CType *ctp, *elem; void *base;
  size_t len, esz, k;
  lua_Integer count, step, idx, last, delta;
  int is_unsigned;
  if (arrv == NULL || ct == NULL || st == NULL || it == NULL ||
      at == NULL || rt == NULL || !ttisfulluserdata(arrv) ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      (!ttisinteger(at) && !ttisfloat(at)))
    return 0;
  u = (Udata *)arrv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  ctp = cd->ct;
  if (ctp == NULL || ctp->kind != CT_ARRAY || ctp->child == NULL)
    return 0;   /* pointers have no length to bound-check */
  elem = ctp->child;
  if (elem->kind == CT_INT &&
      (elem->size == 1 || elem->size == 2 || elem->size == 4)) {
    is_unsigned = elem->is_unsigned;
  }
  else if (elem->kind == CT_ENUM && elem->size == 4) {
    /* Enums are exposed through the unsigned 32-bit numeric view. */
    is_unsigned = 1;
  }
  else if (elem->kind == CT_FLOAT &&
           (elem->size == 4 || elem->size == 8)) {
    is_unsigned = 0;
  }
  else
    return 0;
  esz = elem->size;
  len = ctp->vla ? cd->len : ctp->length;
  base = (cd->flags & (CDATA_OWNED | CDATA_REF)) != 0 ? cd->v.p
                                                      : (void *)&cd->v;
  if (base == NULL)
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step == 0)
    return 0;
  /* The run must stay inside the bounded cdata array.  A monotonic step
  ** keeps every visited key between the two endpoints. */
  if (step > 0) {
    if (count > LUA_MAXINTEGER / step)
      return 0;
  }
  else {
    if (step == LUA_MININTEGER || count > LUA_MAXINTEGER / (-step))
      return 0;
  }
  delta = count * step;
  if ((step > 0 && idx > LUA_MAXINTEGER - delta) ||
      (step < 0 && idx < LUA_MININTEGER - delta))
    return 0;
  last = idx + delta;
  if (idx < 0 || last < 0 ||
      (uint64_t)idx >= (uint64_t)len ||
      (uint64_t)last >= (uint64_t)len)
    return 0;
  if (ttisinteger(at)) {
    lua_Unsigned sum = l_castS2U(ivalue(at));
    lua_Number fsum = 0;
    int promoted = 0;
    const char *p = (const char *)base + (ptrdiff_t)idx * (ptrdiff_t)esz;
    for (k = 0; k <= (size_t)count; k++) {
      if (elem->kind == CT_FLOAT) {
        double d;
        if (esz == 4) {
          float f;
          memcpy(&f, p, sizeof(f));
          d = (double)f;
        }
        else {
          memcpy(&d, p, sizeof(d));
        }
        if (!promoted) {
          fsum = cast_num(l_castU2S(sum));
          promoted = 1;
        }
        if (sub) fsum -= d; else fsum += d;
      }
      else {
        int64_t v = is_unsigned ? (int64_t)trace_load_uint(p, esz)
                                : trace_load_int(p, esz);
        if (promoted) {
          if (sub) fsum -= cast_num(v); else fsum += cast_num(v);
        }
        else if (sub)
          sum -= l_castS2U(v);
        else
          sum += l_castS2U(v);
      }
      p += (ptrdiff_t)step * (ptrdiff_t)esz;
    }
    if (promoted) {
      setfltvalue(at, fsum);
    }
    else {
      chgivalue(at, l_castU2S(sum));
    }
  }
  else {
    lua_Number sum = fltvalue(at);
    const char *p = (const char *)base + (ptrdiff_t)idx * (ptrdiff_t)esz;
    for (k = 0; k <= (size_t)count; k++) {
      if (elem->kind == CT_FLOAT) {
        if (esz == 4) {
          float f;
          memcpy(&f, p, sizeof(f));
          if (sub) sum -= (double)f; else sum += (double)f;
        }
        else {
          double d;
          memcpy(&d, p, sizeof(d));
          if (sub) sum -= d; else sum += d;
        }
      }
      else {
        int64_t v = is_unsigned ? (int64_t)trace_load_uint(p, esz)
                                : trace_load_int(p, esz);
        if (sub) sum -= cast_num(v); else sum += cast_num(v);
      }
      p += (ptrdiff_t)step * (ptrdiff_t)esz;
    }
    setfltvalue(at, sum);
  }
  /* Preserve the element temporary the bytecode wrote on the last body
  ** iteration, like the table intrinsic does. */
  {
    const char *p = (const char *)base + (ptrdiff_t)last * (ptrdiff_t)esz;
    if (elem->kind == CT_FLOAT) {
      if (esz == 4) {
        float f;
        memcpy(&f, p, sizeof(f));
        setfltvalue(rt, (lua_Number)f);
      }
      else {
        double d;
        memcpy(&d, p, sizeof(d));
        setfltvalue(rt, d);
      }
    }
    else if (is_unsigned) {
      setivalue(rt, (lua_Integer)trace_load_uint(p, esz));
    }
    else {
      setivalue(rt, (lua_Integer)trace_load_int(p, esz));
    }
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* ------------------------------------------------------------------ */
/* J2 two-source cdata reductions                                      */
/* ------------------------------------------------------------------ */

/* Lua arithmetic distinguishes wrapped integers from doubles.  The pair
** helpers below reproduce the interpreter's per-operation promotion so
** `s = s + a[i] + b[i]` matches even when an element or the accumulator
** changes flavour in the middle of a run. */
typedef struct {
  int isflt;
  int64_t i;
  double d;
} FfiNum;

typedef struct {
  CType *elem;
  const char *base;
  size_t len;
  size_t esz;
  int isflt;
  int is_unsigned;
  int isbool;   /* store-only C _Bool view: values normalize to 0/1 */
} FfiView;

static FfiNum ffi_num_int (int64_t v) {
  FfiNum n;
  n.isflt = 0;
  n.i = v;
  n.d = 0.0;
  return n;
}

static FfiNum ffi_num_flt (double v) {
  FfiNum n;
  n.isflt = 1;
  n.i = 0;
  n.d = v;
  return n;
}

static double ffi_num_double (FfiNum n) {
  return n.isflt ? n.d : cast_num(n.i);
}

static FfiNum ffi_num_add (FfiNum a, FfiNum b) {
  if (a.isflt || b.isflt)
    return ffi_num_flt(ffi_num_double(a) + ffi_num_double(b));
  return ffi_num_int(l_castU2S(l_castS2U(a.i) + l_castS2U(b.i)));
}

static FfiNum ffi_num_sub (FfiNum a, FfiNum b) {
  if (a.isflt || b.isflt)
    return ffi_num_flt(ffi_num_double(a) - ffi_num_double(b));
  return ffi_num_int(l_castU2S(l_castS2U(a.i) - l_castS2U(b.i)));
}

static FfiNum ffi_num_mul (FfiNum a, FfiNum b) {
  if (a.isflt || b.isflt)
    return ffi_num_flt(ffi_num_double(a) * ffi_num_double(b));
  return ffi_num_int(l_castU2S(l_castS2U(a.i) * l_castS2U(b.i)));
}

/* Integer floor division/modulo with Lua's wrap rules.  A zero divisor is
** never passed here: the store helpers pre-scan it and fall back so the
** interpreter can raise the exact n//0 / n%0 error before any store. */
static int64_t ffi_idiv_int (int64_t m, int64_t n) {
  int64_t q;
  if (n == -1)   /* avoid overflow for mininteger // -1 */
    return l_castU2S(0u - l_castS2U(m));
  q = m / n;
  if (((m ^ n) < 0) && m % n != 0)
    q -= 1;
  return q;
}

static int64_t ffi_mod_int (int64_t m, int64_t n) {
  int64_t r;
  if (n == -1)   /* m % -1 == 0, avoid overflow */
    return 0;
  r = m % n;
  if (r != 0 && ((r ^ n) < 0))
    r += n;
  return r;
}

static FfiNum ffi_num_idiv (FfiNum a, FfiNum b) {
  if (a.isflt || b.isflt)
    return ffi_num_flt(floor(ffi_num_double(a) / ffi_num_double(b)));
  return ffi_num_int(ffi_idiv_int(a.i, b.i));
}

static FfiNum ffi_num_mod (FfiNum a, FfiNum b) {
  if (a.isflt || b.isflt) {
    double m = fmod(ffi_num_double(a), ffi_num_double(b));
    double y = ffi_num_double(b);
    if ((m > 0) ? y < 0 : (m < 0 && y > 0))
      m += y;
    return ffi_num_flt(m);
  }
  return ffi_num_int(ffi_mod_int(a.i, b.i));
}

static FfiNum ffi_num_band (FfiNum a, FfiNum b) {
  return ffi_num_int(a.i & b.i);
}

static FfiNum ffi_num_bor (FfiNum a, FfiNum b) {
  return ffi_num_int(a.i | b.i);
}

static FfiNum ffi_num_bxor (FfiNum a, FfiNum b) {
  return ffi_num_int(a.i ^ b.i);
}

static FfiNum ffi_num_shift (FfiNum a, FfiNum b) {
  int64_t n = b.i;
  if (n < 0) {
    if (n <= -64)
      return ffi_num_int(0);
    return ffi_num_int(l_castU2S(l_castS2U(a.i) >> (unsigned)(-n)));
  }
  if (n >= 64)
    return ffi_num_int(0);
  return ffi_num_int(l_castU2S(l_castS2U(a.i) << (unsigned)n));
}


static void ffi_num_store (TValue *dst, FfiNum n) {
  if (n.isflt) {
    setfltvalue(dst, n.d);
  }
  else {
    setivalue(dst, n.i);
  }
}

/* Bounded cdata array view.  Pointers have no length and are rejected, as
** in the single-array reduction.  Element loads produce Lua numbers, so
** 8-byte integer elements are only accepted when the caller is a pure
** store context (allow64) that never exposes a loaded element as a value;
** a real int64/uint64 read yields cdata, not a Lua integer. */
static int ffi_view_init_ex (TValue *tv, FfiView *v, int allow64) {
  Udata *u;
  CData *cd;
  CType *ct, *elem;
  if (tv == NULL || !ttisfulluserdata(tv))
    return 0;
  u = (Udata *)tv->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0 ||
      cd->ct == NULL || cd->ct->kind != CT_ARRAY || cd->ct->child == NULL)
    return 0;
  ct = cd->ct;
  elem = ct->child;
  v->isbool = 0;
  if (elem->kind == CT_INT &&
      (elem->size == 1 || elem->size == 2 || elem->size == 4 ||
       (allow64 && elem->size == 8))) {
    v->isflt = 0;
    v->is_unsigned = elem->is_unsigned;
  }
  else if (elem->kind == CT_ENUM && elem->size == 4) {
    v->isflt = 0;
    v->is_unsigned = 1;   /* enum reads use the unsigned 32-bit view */
  }
  else if (elem->kind == CT_BOOL && allow64) {
    /* A _Bool read yields a Lua boolean, not a number, so only a pure
    ** store context may use this view.  ffi_convert normalizes any
    ** non-zero scalar to 1. */
    v->isflt = 0;
    v->is_unsigned = 1;
    v->isbool = 1;
  }
  else if (elem->kind == CT_FLOAT &&
           (elem->size == 4 || elem->size == 8)) {
    v->isflt = 1;
    v->is_unsigned = 0;
  }
  else
    return 0;
  v->elem = elem;
  v->esz = elem->size;
  v->len = ct->vla ? cd->len : ct->length;
  v->base = (cd->flags & (CDATA_OWNED | CDATA_REF)) != 0 ? cd->v.p
                                                         : (void *)&cd->v;
  return v->base != NULL;
}

static int ffi_view_init (TValue *tv, FfiView *v) {
  return ffi_view_init_ex(tv, v, 0);
}

/* Store-only destination view.  An 8-byte integer element is safe here
** because the helper never loads it back into a Lua slot: it only applies
** the C store conversion of an arithmetic result. */
static int ffi_view_init_dst (TValue *tv, FfiView *v) {
  return ffi_view_init_ex(tv, v, 1);
}

static FfiNum ffi_view_load (const FfiView *v, const char *p) {
  if (v->isbool)
    return ffi_num_int(0);   /* bool views are store-only */
  if (v->isflt) {
    if (v->esz == 4) {
      float f;
      memcpy(&f, p, sizeof(f));
      return ffi_num_flt((double)f);
    }
    else {
      double d;
      memcpy(&d, p, sizeof(d));
      return ffi_num_flt(d);
    }
  }
  if (v->is_unsigned)
    return ffi_num_int((int64_t)trace_load_uint(p, v->esz));
  return ffi_num_int(trace_load_int(p, v->esz));
}

/* True when an integer view contains a zero in the run.  Used by the // and
** % store helpers to reject before mutating; the interpreter then reports
** the exact division-by-zero error at the failing element. */
static int ffi_view_has_zero (const FfiView *v, lua_Integer count,
                              lua_Integer step, lua_Integer idx) {
  const char *p = v->base + (ptrdiff_t)idx * (ptrdiff_t)v->esz;
  lua_Integer k;
  for (k = 0; k <= count; k++) {
    if (ffi_view_load(v, p).i == 0)
      return 1;
    p += (ptrdiff_t)step * (ptrdiff_t)v->esz;
  }
  return 0;
}

static FfiNum ffi_num_from_slot (TValue *tv) {
  if (ttisfloat(tv))
    return ffi_num_flt(fltvalue(tv));
  return ffi_num_int(ivalue(tv));
}


/* J2: `s = s + a[i] * k` or `s = s + a[i] / k` over one bounded cdata
** array.  op is 0 for multiply and 1 for divide.  Lua's `/` always yields
** a float (including for two integers), while `*` wraps when both operands
** are integers.  The element and product slots keep the last bytecode
** values; the helper never allocates or raises. */
int luar_ffi_trace_scaled_array (TValue *av, TValue *scalarv, TValue *it,
                                 TValue *ct, TValue *st, TValue *at,
                                 TValue *ra, TValue *rt, int op, int sub) {
  FfiView v;
  FfiNum acc, fac, last_elem, last_prod;
  lua_Integer count, step, idx, last, delta, k;
  const char *p;
  if (av == NULL || scalarv == NULL || it == NULL || ct == NULL ||
      st == NULL || at == NULL || ra == NULL || rt == NULL ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      (!ttisinteger(at) && !ttisfloat(at)))
    return 0;
  if (!ffi_view_init(av, &v))
    return 0;
  if (ttisinteger(scalarv))
    fac = ffi_num_int(ivalue(scalarv));
  else if (ttisfloat(scalarv))
    fac = ffi_num_flt(fltvalue(scalarv));
  else
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step == 0)
    return 0;
  if (step > 0) {
    if (count > LUA_MAXINTEGER / step)
      return 0;
  }
  else {
    if (step == LUA_MININTEGER || count > LUA_MAXINTEGER / (-step))
      return 0;
  }
  delta = count * step;
  if ((step > 0 && idx > LUA_MAXINTEGER - delta) ||
      (step < 0 && idx < LUA_MININTEGER - delta))
    return 0;
  last = idx + delta;
  if (idx < 0 || last < 0 ||
      (uint64_t)idx >= (uint64_t)v.len ||
      (uint64_t)last >= (uint64_t)v.len)
    return 0;
  acc = ffi_num_from_slot(at);
  last_elem = ffi_num_int(0);
  last_prod = ffi_num_int(0);
  p = v.base + (ptrdiff_t)idx * (ptrdiff_t)v.esz;
  for (k = 0; k <= count; k++) {
    FfiNum elem = ffi_view_load(&v, p);
    last_elem = elem;
    if (op == 1)
      last_prod = ffi_num_flt(ffi_num_double(elem) / ffi_num_double(fac));
    else
      last_prod = ffi_num_mul(elem, fac);
    acc = sub ? ffi_num_sub(acc, last_prod) : ffi_num_add(acc, last_prod);
    p += (ptrdiff_t)step * (ptrdiff_t)v.esz;
  }
  ffi_num_store(at, acc);
  if (ra != rt)
    ffi_num_store(ra, last_elem);
  ffi_num_store(rt, last_prod);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}

/* J2: `s = s + a[i] + b[i]`, `s = s + a[i] - b[i]`,
** `s = s + a[i] * b[i]`, `s = s + a[i] / b[i]` and the two subtract
** spellings `s = s - a[i] * b[i]` / `s = s - a[i] / b[i]` over two
** bounded cdata arrays.  Both the element temporaries and the
** intermediate/final accumulator registers are left with the values the
** interpreter would hold after the run.  kind is 0 (add), 1 (subtract),
** 2 (multiply add), 3 (divide add), 4 (multiply subtract), 5 (divide
** subtract), 6/7 (floor-divide/modulo add) or 8/9 (their subtract
** spellings). */
int luar_ffi_trace_pair_array (TValue *av, TValue *bv, TValue *it,
                               TValue *ct, TValue *st, TValue *at,
                               TValue *ra, TValue *rb, TValue *rt,
                               int kind) {
  FfiView va, vb;
  FfiNum acc, last_temp, last_a, last_b;
  lua_Integer count, step, idx, last, delta, ckey, k;
  if (av == NULL || bv == NULL || it == NULL || ct == NULL || st == NULL ||
      at == NULL || ra == NULL || rb == NULL || rt == NULL ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      (!ttisinteger(at) && !ttisfloat(at)))
    return 0;
  if (!ffi_view_init(av, &va) || !ffi_view_init(bv, &vb))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step == 0)
    return 0;
  if (step > 0) {
    if (count > LUA_MAXINTEGER / step)
      return 0;
  }
  else {
    if (step == LUA_MININTEGER || count > LUA_MAXINTEGER / (-step))
      return 0;
  }
  delta = count * step;
  if ((step > 0 && idx > LUA_MAXINTEGER - delta) ||
      (step < 0 && idx < LUA_MININTEGER - delta))
    return 0;
  last = idx + delta;
  if (idx < 0 || last < 0 ||
      (uint64_t)idx >= (uint64_t)va.len ||
      (uint64_t)last >= (uint64_t)va.len ||
      (uint64_t)idx >= (uint64_t)vb.len ||
      (uint64_t)last >= (uint64_t)vb.len)
    return 0;
  acc = ffi_num_from_slot(at);
  last_temp = acc;
  last_a = ffi_num_int(0);
  last_b = ffi_num_int(0);
  {
    const char *pa = va.base + (ptrdiff_t)idx * (ptrdiff_t)va.esz;
    const char *pb = vb.base + (ptrdiff_t)idx * (ptrdiff_t)vb.esz;
    for (k = 0, ckey = idx; k <= count; k++, ckey += step) {
      FfiNum x, y;
      (void)ckey;
      x = ffi_view_load(&va, pa);
      y = ffi_view_load(&vb, pb);
      last_a = x;
      last_b = y;
      if (kind >= 2) {
        int div = (kind == 3 || kind == 5);
        int idiv = (kind == 6 || kind == 8);
        int mod = (kind == 7 || kind == 9);
        int sub = (kind == 4 || kind == 5 || kind == 8 || kind == 9);
        FfiNum prod;
        if ((idiv || mod) && !x.isflt && !y.isflt && y.i == 0)
          return 0;   /* integer zero divisor: the interpreter raises */
        if (idiv)
          prod = ffi_num_idiv(x, y);
        else if (mod)
          prod = ffi_num_mod(x, y);
        else if (div)
          prod = ffi_num_flt(ffi_num_double(x) / ffi_num_double(y));
        else
          prod = ffi_num_mul(x, y);
        last_temp = prod;
        acc = sub ? ffi_num_sub(acc, prod) : ffi_num_add(acc, prod);
      }
      else {
        FfiNum t = ffi_num_add(acc, x);
        last_temp = t;
        acc = (kind == 1) ? ffi_num_sub(t, y) : ffi_num_add(t, y);
      }
      pa += (ptrdiff_t)step * (ptrdiff_t)va.esz;
      pb += (ptrdiff_t)step * (ptrdiff_t)vb.esz;
    }
  }
  ffi_num_store(at, acc);
  /* Preserve the bytecode-visible temporaries.  If a temp register is also
  ** the result destination, the interpreter leaves the result there (the
  ** table pair intrinsic follows the same rule). */
  ffi_num_store(rt, last_temp);
  ffi_num_store(ra, (ra == rt) ? last_temp : last_a);
  ffi_num_store(rb, (rb == rt) ? last_temp : last_b);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* ------------------------------------------------------------------ */
/* J2 bounded cdata array stores                                       */
/* ------------------------------------------------------------------ */

/* Convert a Lua numeric slot to the internal FFI number form.  Other
** values (strings, cdata scalars, tables) keep the loop interpreted, so
** the helper never has to raise or allocate. */
static int ffi_num_from_tv (const TValue *tv, FfiNum *n) {
  if (ttisinteger(tv)) {
    *n = ffi_num_int(ivalue(tv));
    return 1;
  }
  if (ttisfloat(tv)) {
    *n = ffi_num_flt(fltvalue(tv));
    return 1;
  }
  return 0;
}


/* Store a Lua arithmetic result into a C element exactly as ffi_convert
** does: integer destinations truncate to their width (with LuaJIT's
** out-of-range double sentinel), float destinations round through the
** declared element size. */
static void ffi_view_store (const FfiView *v, char *p, FfiNum n) {
  if (v->isbool) {
    uint64_t b = n.isflt ? (n.d != 0.0) : (n.i != 0);
    ffi_store_uint(p, 1, b);
  }
  else if (v->isflt) {
    double d = ffi_num_double(n);
    if (v->esz == 4) {
      float f = (float)d;
      memcpy(p, &f, sizeof(f));
    }
    else {
      memcpy(p, &d, sizeof(d));
    }
  }
  else {
    uint64_t u;
    if (n.isflt)
      u = v->is_unsigned ? ffi_double_to_u64(n.d)
                         : (uint64_t)ffi_double_to_i64(n.d);
    else
      u = (uint64_t)n.i;
    ffi_store_uint(p, v->esz, u);
  }
}


/* Validate the interpreter's FORLOOP state and the bounded-array range.
** On success *last is the final visited key (the helper's control-slot
** convention, shared with the reduction intrinsics). */
static int ffi_store_range (const FfiView *v, lua_Integer count,
                            lua_Integer step, lua_Integer idx,
                            lua_Integer *last) {
  lua_Integer delta;
  if (count < 0 || step == 0)
    return 0;
  if (step > 0) {
    if (count > LUA_MAXINTEGER / step)
      return 0;
  }
  else {
    if (step == LUA_MININTEGER || count > LUA_MAXINTEGER / (-step))
      return 0;
  }
  delta = count * step;
  if ((step > 0 && idx > LUA_MAXINTEGER - delta) ||
      (step < 0 && idx < LUA_MININTEGER - delta))
    return 0;
  *last = idx + delta;
  if (idx < 0 || *last < 0 ||
      (uint64_t)idx >= (uint64_t)v->len ||
      (uint64_t)*last >= (uint64_t)v->len)
    return 0;
  return 1;
}


/* J2: `a[i] = v`, `a[i] = i` and `a[i] = constant` over a bounded cdata
** array.  mode 0 reads the numeric slot val, mode 1 writes the loop key
** and mode 2 reads the numeric constant val (a Proto constant). */
int luar_ffi_trace_store_array (TValue *arrv, TValue *it, TValue *ct,
                                TValue *st, TValue *val, int mode) {
  FfiView v;
  FfiNum n = ffi_num_int(0);
  lua_Integer count, step, idx, last, k;
  if (arrv == NULL || it == NULL || ct == NULL || st == NULL ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it))
    return 0;
  if (mode != 1 && (val == NULL || !ffi_num_from_tv(val, &n)))
    return 0;
  /* Pure fills may target 8-byte integer elements: no element is loaded
  ** into a Lua slot, only the C store conversion of a Lua number is used. */
  if (!ffi_view_init_ex(arrv, &v, 1))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!ffi_store_range(&v, count, step, idx, &last))
    return 0;
  {
    char *p = (char *)v.base + (ptrdiff_t)idx * (ptrdiff_t)v.esz;
    lua_Integer ckey = idx;
    for (k = 0; k <= count; k++, ckey += step) {
      FfiNum cur = (mode == 1) ? ffi_num_int(ckey) : n;
      ffi_view_store(&v, p, cur);
      p += (ptrdiff_t)step * (ptrdiff_t)v.esz;
    }
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* J2: `a[i] = b[i]` over two bounded cdata arrays.  The source element is
** converted to the destination element type and the bytecode-visible
** GETTABLE temporary keeps the last source value. */
int luar_ffi_trace_copy_array (TValue *dstv, TValue *srcv, TValue *it,
                               TValue *ct, TValue *st, TValue *tmp) {
  FfiView vd, vs;
  FfiNum lastval;
  lua_Integer count, step, idx, last, k;
  if (dstv == NULL || srcv == NULL || it == NULL || ct == NULL ||
      st == NULL || tmp == NULL || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  if (!ffi_view_init_dst(dstv, &vd) || !ffi_view_init(srcv, &vs))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!ffi_store_range(&vd, count, step, idx, &last) ||
      !ffi_store_range(&vs, count, step, idx, &last))
    return 0;
  lastval = ffi_num_int(0);
  {
    char *pd = (char *)vd.base + (ptrdiff_t)idx * (ptrdiff_t)vd.esz;
    const char *ps = vs.base + (ptrdiff_t)idx * (ptrdiff_t)vs.esz;
    for (k = 0; k <= count; k++) {
      lastval = ffi_view_load(&vs, ps);
      ffi_view_store(&vd, pd, lastval);
      pd += (ptrdiff_t)step * (ptrdiff_t)vd.esz;
      ps += (ptrdiff_t)step * (ptrdiff_t)vs.esz;
    }
  }
  ffi_num_store(tmp, lastval);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* J2: raw 64-bit cdata copy.  The visible element temp is a cdata int64/
** uint64 object; create it once, then copy 8-byte values directly. */
int luar_ffi_trace_copy_array_i64 (TValue *dstv, TValue *srcv, TValue *it,
                                   TValue *ct, TValue *st, TValue *tmp,
                                   int tmp_reg, lua_State *L) {
  FfiView vd, vs;
  lua_Integer count, step, idx, last, k;
  uint64_t lastv = 0;
  CData *cd;
  TValue *top, *base;
  if (dstv == NULL || srcv == NULL || it == NULL || ct == NULL ||
      st == NULL || tmp == NULL || L == NULL ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      tmp_reg < 0 || tmp_reg >= 256)
    return 0;
  if (!ffi_view_init_dst(dstv, &vd) || !ffi_view_init_dst(srcv, &vs) ||
      vd.isflt || vs.isflt || vd.esz != 8 || vs.esz != 8)
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!ffi_store_range(&vd, count, step, idx, &last) ||
      !ffi_store_range(&vs, count, step, idx, &last))
    return 0;
  {
    char *pd = (char *)vd.base + (ptrdiff_t)idx * (ptrdiff_t)vd.esz;
    const char *ps = vs.base + (ptrdiff_t)idx * (ptrdiff_t)vs.esz;
    for (k = 0; k <= count; k++) {
      memcpy(&lastv, ps, 8);
      memcpy(pd, ps, 8);
      pd += (ptrdiff_t)step * (ptrdiff_t)vd.esz;
      ps += (ptrdiff_t)step * (ptrdiff_t)vs.esz;
    }
  }
  cd = ffi_push_scalar(L, ffi_getstate(L), vs.elem);
  memcpy(&cd->v, &lastv, 8);
  top = s2v(L->top.p - 1);
  base = (TValue *)(L->ci->func.p + 1);
  base[tmp_reg] = *top;
  L->top.p--;
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* J2: in-place bounded-array updates.  ops 0..5 are `+ - * / // %`, ops
** 6..10 are the integer bitwise/shift forms `& | ~ << >>` and ops 11..12
** are unary `~`/`-`.  Arithmetic keeps Lua number promotion before the C
** store conversion; the integer forms require an integer-element view and
** follow Lua's integer conversion, wrap and shift rules exactly. */
int luar_ffi_trace_update_array (TValue *arrv, TValue *it, TValue *ct,
                                 TValue *st, TValue *scalar, TValue *tmp,
                                 int op) {
  FfiView v;
  FfiNum n, lastval;
  lua_Integer count, step, idx, last, k;
  lua_Integer iv = 0;
  int needs_int = (op >= 6 && op <= 10);
  if (arrv == NULL || it == NULL || ct == NULL || st == NULL ||
      scalar == NULL || tmp == NULL || !ttisinteger(ct) ||
      !ttisinteger(st) || !ttisinteger(it) || op < 0 || op > 12)
    return 0;
  if (!ffi_num_from_tv(scalar, &n) || !ffi_view_init(arrv, &v))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!ffi_store_range(&v, count, step, idx, &last))
    return 0;
  /* The bitwise forms are only installed for integer element views.  A
  ** non-integral or out-of-range shift/bitwise count makes the interpreter
  ** raise, so decline before the first store. */
  if (op >= 6 && v.isflt)
    return 0;
  if (needs_int && !luaV_tointegerns(scalar, &iv, F2Ieq))
    return 0;
  /* Integer // and % by zero raise; decline before the first store so the
  ** interpreter reports the error at the failing element. */
  if ((op == 4 || op == 5) && !v.isflt && !n.isflt && n.i == 0)
    return 0;
  lastval = ffi_num_int(0);
  {
    char *p = (char *)v.base + (ptrdiff_t)idx * (ptrdiff_t)v.esz;
    for (k = 0; k <= count; k++) {
      FfiNum x = ffi_view_load(&v, p);
      if (op == 0)
        lastval = ffi_num_add(x, n);
      else if (op == 1)
        lastval = ffi_num_sub(x, n);
      else if (op == 2)
        lastval = ffi_num_mul(x, n);
      else if (op == 3)
        lastval = ffi_num_flt(ffi_num_double(x) / ffi_num_double(n));
      else if (op == 4)
        lastval = ffi_num_idiv(x, n);
      else if (op == 5)
        lastval = ffi_num_mod(x, n);
      else if (op == 6)
        lastval = ffi_num_int(l_castU2S(l_castS2U(x.i) &
                                        l_castS2U(iv)));
      else if (op == 7)
        lastval = ffi_num_int(l_castU2S(l_castS2U(x.i) |
                                        l_castS2U(iv)));
      else if (op == 8)
        lastval = ffi_num_int(l_castU2S(l_castS2U(x.i) ^
                                        l_castS2U(iv)));
      else if (op == 9)
        lastval = ffi_num_int(luaV_shiftl(x.i, iv));
      else if (op == 10)
        lastval = ffi_num_int(luaV_shiftl(x.i,
                                          l_castU2S(0 - l_castS2U(iv))));
      else if (op == 11)
        lastval = ffi_num_int(l_castU2S(~l_castS2U(x.i)));
      else
        lastval = ffi_num_int(l_castU2S(0 - l_castS2U(x.i)));
      ffi_view_store(&v, p, lastval);
      p += (ptrdiff_t)step * (ptrdiff_t)v.esz;
    }
  }
  ffi_num_store(tmp, lastval);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* J2: `a[i] = b[i] + v` and the subtract/multiply/divide/floor-divide/
** modulo forms over two bounded arrays.  op is 0..5.  As with the in-place
** update, the arithmetic is performed with Lua number promotion first,
** then the C element-store conversion is applied.  The bytecode result
** temporary keeps the last computed result. */
int luar_ffi_trace_scalar_store (TValue *dstv, TValue *srcv, TValue *it,
                                 TValue *ct, TValue *st, TValue *scalar,
                                 TValue *tmp, int op) {
  FfiView vd, vs;
  FfiNum n, lastres;
  lua_Integer count, step, idx, last, k;
  if (dstv == NULL || srcv == NULL || it == NULL || ct == NULL ||
      st == NULL || scalar == NULL || tmp == NULL ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      op < 0 || op > 10)
    return 0;
  if (!ffi_num_from_tv(scalar, &n) ||
      !ffi_view_init_dst(dstv, &vd) || !ffi_view_init(srcv, &vs))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!ffi_store_range(&vd, count, step, idx, &last) ||
      !ffi_store_range(&vs, count, step, idx, &last))
    return 0;
  /* Integer // and % by zero raise; decline before the first store. */
  if ((op == 4 || op == 5) && !vs.isflt && !n.isflt && n.i == 0)
    return 0;
  if (op >= 6 && (vs.isflt || n.isflt))
    return 0;   /* bitwise operands must be integers */
  lastres = ffi_num_int(0);
  {
    char *pd = (char *)vd.base + (ptrdiff_t)idx * (ptrdiff_t)vd.esz;
    const char *ps = vs.base + (ptrdiff_t)idx * (ptrdiff_t)vs.esz;
    for (k = 0; k <= count; k++) {
      FfiNum x = ffi_view_load(&vs, ps);
      if (op == 0)
        lastres = ffi_num_add(x, n);
      else if (op == 1)
        lastres = ffi_num_sub(x, n);
      else if (op == 2)
        lastres = ffi_num_mul(x, n);
      else if (op == 3)
        lastres = ffi_num_flt(ffi_num_double(x) / ffi_num_double(n));
      else if (op == 4)
        lastres = ffi_num_idiv(x, n);
      else if (op == 5)
        lastres = ffi_num_mod(x, n);
      else if (op == 6)
        lastres = ffi_num_band(x, n);
      else if (op == 7)
        lastres = ffi_num_bor(x, n);
      else if (op == 8)
        lastres = ffi_num_bxor(x, n);
      else if (op == 9)
        lastres = ffi_num_shift(x, n);
      else {
        if (n.i == INT64_MIN)
          lastres = ffi_num_int(0);
        else
          lastres = ffi_num_shift(x, ffi_num_int(-n.i));
      }
      ffi_view_store(&vd, pd, lastres);
      pd += (ptrdiff_t)step * (ptrdiff_t)vd.esz;
      ps += (ptrdiff_t)step * (ptrdiff_t)vs.esz;
    }
  }
  ffi_num_store(tmp, lastres);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* J2: `a[i] = -b[i]` and `a[i] = ~b[i]` over bounded cdata arrays. */
int luar_ffi_trace_unary_store (TValue *dstv, TValue *srcv, TValue *it,
                                TValue *ct, TValue *st, TValue *elem,
                                TValue *res, int op) {
  FfiView vd, vs;
  FfiNum lastres, lastelem;
  lua_Integer count, step, idx, last, k;
  if (dstv == NULL || srcv == NULL || it == NULL || ct == NULL ||
      st == NULL || elem == NULL || res == NULL || !ttisinteger(ct) ||
      !ttisinteger(st) || !ttisinteger(it) || (op != 0 && op != 1))
    return 0;
  if (!ffi_view_init_dst(dstv, &vd) || !ffi_view_init(srcv, &vs))
    return 0;
  if (op == 1 && vs.isflt)
    return 0;   /* bitwise not requires integer elements */
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!ffi_store_range(&vd, count, step, idx, &last) ||
      !ffi_store_range(&vs, count, step, idx, &last))
    return 0;
  lastres = ffi_num_int(0);
  lastelem = ffi_num_int(0);
  {
    char *pd = (char *)vd.base + (ptrdiff_t)idx * (ptrdiff_t)vd.esz;
    const char *ps = vs.base + (ptrdiff_t)idx * (ptrdiff_t)vs.esz;
    for (k = 0; k <= count; k++) {
      FfiNum x = ffi_view_load(&vs, ps);
      lastelem = x;
      if (op == 0)
        lastres = x.isflt ? ffi_num_flt(-x.d)
                          : ffi_num_int(l_castU2S(0u - l_castS2U(x.i)));
      else
        lastres = ffi_num_int(~x.i);
      ffi_view_store(&vd, pd, lastres);
      pd += (ptrdiff_t)step * (ptrdiff_t)vd.esz;
      ps += (ptrdiff_t)step * (ptrdiff_t)vs.esz;
    }
  }
  ffi_num_store(elem, lastelem);
  ffi_num_store(res, lastres);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* J2: `a[i] = b[i] + c[i]` and the subtract/multiply/divide/floor-divide/
** modulo/bitwise forms over three bounded arrays.  op is 0..10.  tmpa is the first
** GETTABLE/result temporary and tmpb the second source temporary; the
** recognizer only accepts shapes whose arithmetic destination is tmpa. */
int luar_ffi_trace_pair_store (TValue *dstv, TValue *av, TValue *bv,
                               TValue *it, TValue *ct, TValue *st,
                               TValue *tmpa, TValue *tmpb, int op) {
  FfiView vd, va, vb;
  FfiNum lasta, lastb, lastres;
  lua_Integer count, step, idx, last, k;
  if (dstv == NULL || av == NULL || bv == NULL || it == NULL ||
      ct == NULL || st == NULL || tmpa == NULL || tmpb == NULL ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      op < 0 || op > 10)
    return 0;
  if (!ffi_view_init_dst(dstv, &vd) || !ffi_view_init(av, &va) ||
      !ffi_view_init(bv, &vb))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!ffi_store_range(&vd, count, step, idx, &last) ||
      !ffi_store_range(&va, count, step, idx, &last) ||
      !ffi_store_range(&vb, count, step, idx, &last))
    return 0;
  /* Integer // and % by zero raise, and the helper must not leave partial
  ** stores behind.  Scan the divisor first; a float operand takes the
  ** non-raising float path instead. */
  if ((op == 4 || op == 5) && !va.isflt && !vb.isflt &&
      ffi_view_has_zero(&vb, count, step, idx))
    return 0;
  if (op >= 6 && (va.isflt || vb.isflt))
    return 0;   /* bitwise operands must be integers */
  lasta = lastb = lastres = ffi_num_int(0);
  {
    char *pd = (char *)vd.base + (ptrdiff_t)idx * (ptrdiff_t)vd.esz;
    const char *pa = va.base + (ptrdiff_t)idx * (ptrdiff_t)va.esz;
    const char *pb = vb.base + (ptrdiff_t)idx * (ptrdiff_t)vb.esz;
    for (k = 0; k <= count; k++) {
      lasta = ffi_view_load(&va, pa);
      lastb = ffi_view_load(&vb, pb);
      if (op == 0)
        lastres = ffi_num_add(lasta, lastb);
      else if (op == 1)
        lastres = ffi_num_sub(lasta, lastb);
      else if (op == 2)
        lastres = ffi_num_mul(lasta, lastb);
      else if (op == 3)
        lastres = ffi_num_flt(ffi_num_double(lasta) /
                              ffi_num_double(lastb));
      else if (op == 4)
        lastres = ffi_num_idiv(lasta, lastb);
      else if (op == 5)
        lastres = ffi_num_mod(lasta, lastb);
      else if (op == 6)
        lastres = ffi_num_band(lasta, lastb);
      else if (op == 7)
        lastres = ffi_num_bor(lasta, lastb);
      else if (op == 8)
        lastres = ffi_num_bxor(lasta, lastb);
      else if (op == 9)
        lastres = ffi_num_shift(lasta, lastb);
      else {
        if (lastb.i == INT64_MIN)
          lastres = ffi_num_int(0);
        else
          lastres = ffi_num_shift(lasta, ffi_num_int(-lastb.i));
      }
      ffi_view_store(&vd, pd, lastres);
      pd += (ptrdiff_t)step * (ptrdiff_t)vd.esz;
      pa += (ptrdiff_t)step * (ptrdiff_t)va.esz;
      pb += (ptrdiff_t)step * (ptrdiff_t)vb.esz;
    }
  }
  ffi_num_store(tmpa, lastres);
  ffi_num_store(tmpb, lastb);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


int luar_ffi_trace_i64_scalar (TValue *tv) {
  Udata *u; CData *cd;
  if (tv == NULL || !ttisfulluserdata(tv)) return 0;
  u = (Udata *)tv->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0)
    return 0;
  return cd->ct != NULL && cd->ct->kind == CT_INT && cd->ct->size == 8;
}

int luar_ffi_trace_fill_array_i64 (TValue *dstv, TValue *it, TValue *ct,
                                  TValue *st, TValue *val) {
  FfiView vd;
  Udata *u;
  CData *cd;
  uint64_t bits;
  lua_Integer count, step, idx, last, k;
  if (dstv == NULL || it == NULL || ct == NULL || st == NULL || val == NULL ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      !luar_ffi_trace_i64_scalar(val))
    return 0;
  if (!ffi_view_init_dst(dstv, &vd) || vd.isflt || vd.esz != 8)
    return 0;
  u = (Udata *)val->value_.gc;
  cd = (CData *)getudatamem(u);
  if ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0)
    memcpy(&bits, cd->v.p, 8);
  else
    memcpy(&bits, &cd->v, 8);
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!ffi_store_range(&vd, count, step, idx, &last))
    return 0;
  {
    char *pd = (char *)vd.base + (ptrdiff_t)idx * (ptrdiff_t)vd.esz;
    for (k = 0; k <= count; k++) {
      memcpy(pd, &bits, 8);
      pd += (ptrdiff_t)step * (ptrdiff_t)vd.esz;
    }
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


int luar_ffi_trace_i64_array (TValue *tv) {
  Udata *u; CData *cd; CType *ct, *elem;
  if (tv == NULL || !ttisfulluserdata(tv)) return 0;
  u = (Udata *)tv->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0) return 0;
  ct = cd->ct;
  if (ct == NULL) return 0;
  if (ct->kind != CT_ARRAY && ct->kind != CT_POINTER) return 0;
  elem = ct->child;
  return elem != NULL && elem->kind == CT_INT && elem->size == 8;
}

int luar_ffi_trace_i64_unsigned (TValue *tv) {
  Udata *u; CData *cd; CType *ct, *elem;
  if (tv == NULL || !ttisfulluserdata(tv)) return 0;
  u = (Udata *)tv->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0) return 0;
  ct = cd->ct;
  if (ct == NULL) return 0;
  if (ct->kind != CT_ARRAY && ct->kind != CT_POINTER) return 0;
  elem = ct->child;
  if (elem == NULL || elem->kind != CT_INT || elem->size != 8) return 0;
  return elem->is_unsigned != 0;
}

int64_t luar_ffi_trace_set_i64 (TValue *arrv, TValue *key, TValue *val) {
  Udata *u; CData *cd; CType *ct, *elem; void *base; lua_Integer idx;
  size_t len = 0; uint64_t v;
  if (arrv == NULL || key == NULL || val == NULL ||
      !ttisfulluserdata(arrv) || !ttisinteger(key) || !ttisinteger(val))
    return 0;
  u = (Udata *)arrv->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0) return 0;
  ct = cd->ct; if (ct == NULL) return 0;
  if (ct->kind != CT_ARRAY && ct->kind != CT_POINTER) return 0;
  elem = ct->child;
  if (elem == NULL || elem->kind != CT_INT || elem->size != 8) return 0;
  base = (ct->kind == CT_POINTER) ? cd->v.p :
         ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ? cd->v.p :
         (void *)&cd->v;
  if (base == NULL) return 0;
  if (ct->kind == CT_ARRAY) len = ct->vla ? cd->len : ct->length;
  idx = ivalue(key);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  v = (uint64_t)ivalue(val);
  memcpy((char *)base + (ptrdiff_t)idx * 8, &v, 8);
  return 1;
}


static int64_t bitop_i64 (TValue *arrv, TValue *key, int64_t factor, int op) {
  Udata *u; CData *cd; CType *ct, *elem; void *base; lua_Integer idx;
  size_t len = 0; int64_t v;
  if (arrv == NULL || key == NULL || !ttisfulluserdata(arrv) ||
      !ttisinteger(key)) return 0;
  u = (Udata *)arrv->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0) return 0;
  ct = cd->ct; if (ct == NULL) return 0;
  if (ct->kind != CT_ARRAY && ct->kind != CT_POINTER) return 0;
  elem = ct->child;
  if (elem == NULL || elem->kind != CT_INT || elem->size != 8) return 0;
  base = (ct->kind == CT_POINTER) ? cd->v.p :
         ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ? cd->v.p :
         (void *)&cd->v;
  if (base == NULL) return 0;
  if (ct->kind == CT_ARRAY) len = ct->vla ? cd->len : ct->length;
  idx = ivalue(key);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  memcpy(&v, (char *)base + (ptrdiff_t)idx * 8, 8);
  if (op == 0) v = v & factor;
  else if (op == 1) v = v | factor;
  else if (op == 2) v = v ^ factor;
  else if (op == 3 || op == 4) {
    /* cdata shifts use the low six bits of the count and are logical. */
    unsigned sh = (unsigned)((uint64_t)factor & 63u);
    v = (int64_t)(op == 3 ? ((uint64_t)v << sh) : ((uint64_t)v >> sh));
  }
  else if (op == 5)
    v = (int64_t)(0 - (uint64_t)v);
  else
    v = ~v;
  memcpy((char *)base + (ptrdiff_t)idx * 8, &v, 8);
  return 1;
}

int64_t luar_ffi_trace_band_i64 (TValue *a, TValue *k, int64_t f) {
  return bitop_i64(a, k, f, 0);
}
int64_t luar_ffi_trace_bor_i64 (TValue *a, TValue *k, int64_t f) {
  return bitop_i64(a, k, f, 1);
}
int64_t luar_ffi_trace_bxor_i64 (TValue *a, TValue *k, int64_t f) {
  return bitop_i64(a, k, f, 2);
}
int64_t luar_ffi_trace_shl_i64 (TValue *a, TValue *k, int64_t c) {
  return bitop_i64(a, k, c, 3);
}
int64_t luar_ffi_trace_shr_i64 (TValue *a, TValue *k, int64_t c) {
  return bitop_i64(a, k, c, 4);
}
int64_t luar_ffi_trace_neg_i64 (TValue *a, TValue *k) {
  return bitop_i64(a, k, 0, 5);
}
int64_t luar_ffi_trace_bnot_i64 (TValue *a, TValue *k) {
  return bitop_i64(a, k, 0, 6);
}


static int64_t divmod_i64 (TValue *arrv, TValue *key, int64_t divisor,
                           int op, int uns) {
  Udata *u; CData *cd; CType *ct, *elem; void *base; lua_Integer idx;
  size_t len = 0; int64_t v;
  if (arrv == NULL || key == NULL || !ttisfulluserdata(arrv) ||
      !ttisinteger(key) || divisor == 0) return 0;
  u = (Udata *)arrv->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0) return 0;
  ct = cd->ct; if (ct == NULL) return 0;
  if (ct->kind != CT_ARRAY && ct->kind != CT_POINTER) return 0;
  elem = ct->child;
  if (elem == NULL || elem->kind != CT_INT || elem->size != 8) return 0;
  base = (ct->kind == CT_POINTER) ? cd->v.p :
         ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ? cd->v.p :
         (void *)&cd->v;
  if (base == NULL) return 0;
  if (ct->kind == CT_ARRAY) len = ct->vla ? cd->len : ct->length;
  idx = ivalue(key);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  memcpy(&v, (char *)base + (ptrdiff_t)idx * 8, 8);
  if (uns) {
    uint64_t a = (uint64_t)v, b = (uint64_t)divisor;
    v = (int64_t)(op == 0 ? (a % b) : (a / b));
  }
  else {
    /* cdata signed division/modulo truncate toward zero, like C.  C
    ** leaves INT64_MIN / -1 undefined (and x86 idiv faults); cdata
    ** semantics wrap the quotient and return a zero remainder. */
    if (divisor == -1)
      v = (op == 0) ? 0 : (int64_t)(0 - (uint64_t)v);
    else
      v = (op == 0) ? (v % divisor) : (v / divisor);
  }
  memcpy((char *)base + (ptrdiff_t)idx * 8, &v, 8);
  return 1;
}


int64_t luar_ffi_trace_mod_i64 (TValue *arrv, TValue *key, int64_t divisor) {
  return divmod_i64(arrv, key, divisor, 0, 0);
}


int64_t luar_ffi_trace_umod_i64 (TValue *arrv, TValue *key, int64_t divisor) {
  return divmod_i64(arrv, key, divisor, 0, 1);
}


int64_t luar_ffi_trace_div_i64 (TValue *arrv, TValue *key, int64_t divisor) {
  return divmod_i64(arrv, key, divisor, 1, 0);
}


int64_t luar_ffi_trace_udiv_i64 (TValue *arrv, TValue *key, int64_t divisor) {
  return divmod_i64(arrv, key, divisor, 1, 1);
}


int64_t luar_ffi_trace_mul_i64 (TValue *arrv, TValue *key, int64_t factor) {
  Udata *u; CData *cd; CType *ct, *elem; void *base; lua_Integer idx;
  size_t len = 0; uint64_t v;
  if (arrv == NULL || key == NULL || !ttisfulluserdata(arrv) ||
      !ttisinteger(key)) return 0;
  u = (Udata *)arrv->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0) return 0;
  ct = cd->ct; if (ct == NULL) return 0;
  if (ct->kind != CT_ARRAY && ct->kind != CT_POINTER) return 0;
  elem = ct->child;
  if (elem == NULL || elem->kind != CT_INT || elem->size != 8) return 0;
  base = (ct->kind == CT_POINTER) ? cd->v.p :
         ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ? cd->v.p :
         (void *)&cd->v;
  if (base == NULL) return 0;
  if (ct->kind == CT_ARRAY) len = ct->vla ? cd->len : ct->length;
  idx = ivalue(key);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  memcpy(&v, (char *)base + (ptrdiff_t)idx * 8, 8);
  /* cdata int64 arithmetic wraps; unsigned math keeps C overflow UB out. */
  v = v * (uint64_t)factor;
  memcpy((char *)base + (ptrdiff_t)idx * 8, &v, 8);
  return 1;
}


int64_t luar_ffi_trace_add_i64 (TValue *arrv, TValue *key, int64_t delta) {
  Udata *u; CData *cd; CType *ct, *elem; void *base; lua_Integer idx;
  size_t len = 0; uint64_t v;
  if (arrv == NULL || key == NULL || !ttisfulluserdata(arrv) ||
      !ttisinteger(key)) return 0;
  u = (Udata *)arrv->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_CTYPE) != 0) return 0;
  ct = cd->ct; if (ct == NULL) return 0;
  if (ct->kind != CT_ARRAY && ct->kind != CT_POINTER) return 0;
  elem = ct->child;
  if (elem == NULL || elem->kind != CT_INT || elem->size != 8) return 0;
  base = (ct->kind == CT_POINTER) ? cd->v.p :
         ((cd->flags & (CDATA_OWNED | CDATA_REF)) != 0) ? cd->v.p :
         (void *)&cd->v;
  if (base == NULL) return 0;
  if (ct->kind == CT_ARRAY) len = ct->vla ? cd->len : ct->length;
  idx = ivalue(key);
  if (ct->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)len))
    return 0;
  memcpy(&v, (char *)base + (ptrdiff_t)idx * 8, 8);
  v = v + (uint64_t)delta;
  memcpy((char *)base + (ptrdiff_t)idx * 8, &v, 8);
  return 1;
}


int64_t luar_ffi_trace_copy_i64 (TValue *dstv, TValue *srcv, TValue *key) {
  Udata *du, *su; CData *d, *s; CType *dt, *st, *de, *se; void *db, *sb;
  lua_Integer idx; size_t dlen = 0, slen = 0;
  if (dstv == NULL || srcv == NULL || key == NULL ||
      !ttisfulluserdata(dstv) || !ttisfulluserdata(srcv) || !ttisinteger(key))
    return 0;
  du = (Udata *)dstv->value_.gc; d = (CData *)getudatamem(du);
  su = (Udata *)srcv->value_.gc; s = (CData *)getudatamem(su);
  if (d->magic != LUAR_FFI_MAGIC || s->magic != LUAR_FFI_MAGIC) return 0;
  if ((d->flags & CDATA_CTYPE) != 0 || (s->flags & CDATA_CTYPE) != 0)
    return 0;
  dt = d->ct; st = s->ct;
  if (dt == NULL || st == NULL) return 0;
  if ((dt->kind != CT_ARRAY && dt->kind != CT_POINTER) ||
      (st->kind != CT_ARRAY && st->kind != CT_POINTER)) return 0;
  de = dt->child; se = st->child;
  if (de == NULL || se == NULL || de->kind != CT_INT || de->size != 8 ||
      se->kind != CT_INT || se->size != 8) return 0;
  /* Match cdata indexing: arrays use their value storage, pointers use
  ** the address stored in v.p. */
  db = (dt->kind == CT_POINTER) ? d->v.p :
       ((d->flags & (CDATA_OWNED | CDATA_REF)) != 0) ? d->v.p :
       (void *)&d->v;
  sb = (st->kind == CT_POINTER) ? s->v.p :
       ((s->flags & (CDATA_OWNED | CDATA_REF)) != 0) ? s->v.p :
       (void *)&s->v;
  if (db == NULL || sb == NULL) return 0;
  if (dt->kind == CT_ARRAY) dlen = dt->vla ? d->len : dt->length;
  if (st->kind == CT_ARRAY) slen = st->vla ? s->len : st->length;
  idx = ivalue(key);
  if (dt->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)dlen))
    return 0;
  if (st->kind == CT_ARRAY && (idx < 0 || (uint64_t)idx >= (uint64_t)slen))
    return 0;
  /* memmove: ffi.copy permits overlapping ranges and src == dst. */
  memmove((char *)db + (ptrdiff_t)idx * 8,
          (char *)sb + (ptrdiff_t)idx * 8, 8);
  return 1;
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
      (elem->size != 1 && elem->size != 2 && elem->size != 4))
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
      (elem->size != 1 && elem->size != 2 && elem->size != 4))
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
        (ft->size == 1 || ft->size == 2 || ft->size == 4))
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
        (ft->size == 1 || ft->size == 2 || ft->size == 4))
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
        (ft->size == 1 || ft->size == 2 || ft->size == 4))
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
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0 ||
      (cd->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      cd->v.p == NULL || cd->ct == NULL)
    return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 0 || ft->vararg)
    return 0;
  rt = ft->ret;
  if (rt == NULL)
    return 0;
  /* A 64-bit integer result is cdata int64/uint64 in the interpreter, not
  ** a Lua integer.  The J4 scalar-call ops materialise Lua integers, so
  ** leave those calls on the interpreter instead of changing the result
  ** type (and precision) at the trace boundary. */
  if (rt->kind == CT_INT &&
      (rt->size == 1 || rt->size == 2 || rt->size == 4))
    return 1;
  if (rt->kind == CT_FLOAT && (rt->size == 4 || rt->size == 8))
    return 2;
  return 0;
}


int64_t luar_ffi_trace_ccall0_double (TValue *fn, TValue *out) {
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
  if (!ffi_trace_call0(cd, &kind, &iv, &dv) || kind != 2)
    return 0;
  setfltvalue(out, dv);
  return 1;
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
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0 ||
      (cd->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      cd->v.p == NULL || cd->ct == NULL)
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
  /* The helper returns a Lua integer; 64-bit C results are cdata. */
  if (rt->size != 1 && rt->size != 2 && rt->size != 4)
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
int luar_ffi_trace_ccall3idd_kind (TValue *fn) {
  Udata *u; CData *cd; CType *ft, *a0, *a1, *a2, *rt;
  if (fn == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 3 || ft->vararg) return 0;
  a0 = ft->args[0]; a1 = ft->args[1]; a2 = ft->args[2]; rt = ft->ret;
  if (a0 == NULL || a1 == NULL || a2 == NULL || rt == NULL ||
      a0->kind != CT_INT || a0->size != 4 ||
      a1->kind != CT_FLOAT || a1->size != 8 ||
      a2->kind != CT_FLOAT || a2->size != 8 ||
      rt->kind != CT_FLOAT || rt->size != 8) return 0;
  return 1;
}

int64_t luar_ffi_trace_ccall3idd (TValue *fn, int64_t a0, double a1,
                                  double a2, TValue *out) {
  Udata *u; CData *cd; double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  if (!ffi_trace_call3idd(cd, a0, a1, a2, &v)) return 0;
  setfltvalue(out, v); return 1;
}

int luar_ffi_trace_ccall3did_kind (TValue *fn) {
  Udata *u; CData *cd; CType *ft, *a0, *a1, *a2, *rt;
  if (fn == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 3 || ft->vararg) return 0;
  a0 = ft->args[0]; a1 = ft->args[1]; a2 = ft->args[2]; rt = ft->ret;
  if (a0 == NULL || a1 == NULL || a2 == NULL || rt == NULL ||
      a0->kind != CT_FLOAT || a0->size != 8 ||
      a1->kind != CT_INT || a1->size != 4 ||
      a2->kind != CT_FLOAT || a2->size != 8 ||
      rt->kind != CT_FLOAT || rt->size != 8) return 0;
  return 1;
}

int64_t luar_ffi_trace_ccall3did (TValue *fn, double a0, int64_t a1,
                                  double a2, TValue *out) {
  Udata *u; CData *cd; double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  if (!ffi_trace_call3did(cd, a0, a1, a2, &v)) return 0;
  setfltvalue(out, v); return 1;
}

int luar_ffi_trace_ccall3ddi_kind (TValue *fn) {
  Udata *u; CData *cd; CType *ft, *a0, *a1, *a2, *rt;
  if (fn == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 3 || ft->vararg) return 0;
  a0 = ft->args[0]; a1 = ft->args[1]; a2 = ft->args[2]; rt = ft->ret;
  if (a0 == NULL || a1 == NULL || a2 == NULL || rt == NULL ||
      a0->kind != CT_FLOAT || a0->size != 8 ||
      a1->kind != CT_FLOAT || a1->size != 8 ||
      a2->kind != CT_INT || a2->size != 4 ||
      rt->kind != CT_FLOAT || rt->size != 8) return 0;
  return 1;
}

int64_t luar_ffi_trace_ccall3ddi (TValue *fn, double a0, double a1,
                                  int64_t a2, TValue *out) {
  Udata *u; CData *cd; double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  if (!ffi_trace_call3di(cd, a0, a1, a2, &v)) return 0;
  setfltvalue(out, v); return 1;
}

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


int luar_ffi_trace_ccall2id_kind (TValue *fn) {
  Udata *u; CData *cd; CType *ft, *r;
  if (fn == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg) return 0;
  r = ft->ret;
  if (r == NULL || r->kind != CT_FLOAT || r->size != 8 ||
      ft->args[0] == NULL || ft->args[1] == NULL ||
      ft->args[0]->kind != CT_INT || ft->args[0]->size != 4 ||
      ft->args[1]->kind != CT_FLOAT || ft->args[1]->size != 8) return 0;
  return 1;
}

int64_t luar_ffi_trace_ccall2id (TValue *fn, int64_t a0, double a1, TValue *out) {
  Udata *u; CData *cd; double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  if (!ffi_trace_call2id(cd, a0, a1, &v)) return 0;
  setfltvalue(out, v);
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


int64_t luar_ffi_trace_ccall1ff (TValue *fn, double arg, TValue *out) {
  Udata *u;
  CData *cd;
  double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC)
    return 0;
  if (!ffi_trace_call1f(cd, arg, &v))
    return 0;
  setfltvalue(out, v);
  return 1;
}


int luar_ffi_trace_ccall1ff_kind (TValue *fn) {
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
      at->size != 4 || rt->kind != CT_FLOAT || rt->size != 4)
    return 0;
  return 1;
}


int luar_ffi_trace_ccall2df_kind (TValue *fn) {
  Udata *u; CData *cd; CType *ft, *r;
  if (fn == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg) return 0;
  r = ft->ret;
  if (r == NULL || r->kind != CT_FLOAT || r->size != 8 ||
      ft->args[0] == NULL || ft->args[1] == NULL ||
      ft->args[0]->kind != CT_FLOAT || ft->args[0]->size != 8 ||
      ft->args[1]->kind != CT_FLOAT || ft->args[1]->size != 4) return 0;
  return 1;
}

int64_t luar_ffi_trace_ccall2df (TValue *fn, double a0, double a1, TValue *out) {
  Udata *u; CData *cd; double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  if (!ffi_trace_call2df(cd, a0, a1, &v)) return 0;
  setfltvalue(out, v);
  return 1;
}

int luar_ffi_trace_ccall2fd_kind (TValue *fn) {
  Udata *u; CData *cd; CType *ft, *r;
  if (fn == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg) return 0;
  r = ft->ret;
  if (r == NULL || r->kind != CT_FLOAT || r->size != 8 ||
      ft->args[0] == NULL || ft->args[1] == NULL ||
      ft->args[0]->kind != CT_FLOAT || ft->args[0]->size != 4 ||
      ft->args[1]->kind != CT_FLOAT || ft->args[1]->size != 8) return 0;
  return 1;
}

int64_t luar_ffi_trace_ccall2fd (TValue *fn, double a0, double a1, TValue *out) {
  Udata *u; CData *cd; double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  if (!ffi_trace_call2fd(cd, a0, a1, &v)) return 0;
  setfltvalue(out, v);
  return 1;
}

int luar_ffi_trace_ccall2if_kind (TValue *fn) {
  Udata *u; CData *cd; CType *ft, *r;
  if (fn == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg) return 0;
  r = ft->ret;
  if (r == NULL || r->kind != CT_FLOAT || r->size != 4 ||
      ft->args[0] == NULL || ft->args[1] == NULL ||
      ft->args[0]->kind != CT_INT || ft->args[0]->size != 4 ||
      ft->args[1]->kind != CT_FLOAT || ft->args[1]->size != 4) return 0;
  return 1;
}

int64_t luar_ffi_trace_ccall2if (TValue *fn, int64_t a0, double a1, TValue *out) {
  Udata *u; CData *cd; double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  if (!ffi_trace_call2if(cd, a0, a1, &v)) return 0;
  setfltvalue(out, v);
  return 1;
}

int luar_ffi_trace_ccall2fi_kind (TValue *fn) {
  Udata *u; CData *cd; CType *ft, *r;
  if (fn == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg) return 0;
  r = ft->ret;
  if (r == NULL || r->kind != CT_FLOAT || r->size != 4 ||
      ft->args[0] == NULL || ft->args[1] == NULL ||
      ft->args[0]->kind != CT_FLOAT || ft->args[0]->size != 4 ||
      ft->args[1]->kind != CT_INT || ft->args[1]->size != 4) return 0;
  return 1;
}

int64_t luar_ffi_trace_ccall2fi (TValue *fn, double a0, int64_t a1, TValue *out) {
  Udata *u; CData *cd; double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn)) return 0;
  u = (Udata *)fn->value_.gc; cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC || (cd->flags & CDATA_FUNC) == 0) return 0;
  if (!ffi_trace_call2fi(cd, a0, a1, &v)) return 0;
  setfltvalue(out, v);
  return 1;
}

int luar_ffi_trace_ccall2ff_kind (TValue *fn) {
  Udata *u;
  CData *cd;
  CType *ft, *r;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC ||
      (cd->flags & CDATA_FUNC) == 0 ||
      cd->v.p == NULL || cd->ct == NULL)
    return 0;
  ft = (cd->ct->kind == CT_FUNC) ? cd->ct : cd->ct->child;
  if (ft == NULL || ft->kind != CT_FUNC || ft->nargs != 2 || ft->vararg)
    return 0;
  r = ft->ret;
  if (r == NULL || r->kind != CT_FLOAT || r->size != 4 ||
      ft->args[0] == NULL || ft->args[1] == NULL ||
      ft->args[0]->kind != CT_FLOAT || ft->args[0]->size != 4 ||
      ft->args[1]->kind != CT_FLOAT || ft->args[1]->size != 4)
    return 0;
  return 1;
}


int64_t luar_ffi_trace_ccall2ff (TValue *fn, double a0, double a1,
                                 TValue *out) {
  Udata *u;
  CData *cd;
  double v;
  if (fn == NULL || out == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  if (cd->magic != LUAR_FFI_MAGIC ||
      (cd->flags & (CDATA_CTYPE | CDATA_CALLBACK |
                    CDATA_OWNED | CDATA_REF)) != 0 ||
      cd->v.p == NULL || cd->ct == NULL)
    return 0;
  if (!ffi_trace_call2f(cd, a0, a1, &v))
    return 0;
  setfltvalue(out, v);
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
          (ft->size == 1 || ft->size == 2 || ft->size == 4))
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
        (ft->size == 1 || ft->size == 2 || ft->size == 4))
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
        (ft->size == 1 || ft->size == 2 || ft->size == 4))
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
    /* The pointer-field helpers below read/write 4-byte ints (CPGETI/
    ** CPSETI) or 8-byte values (CPGETI64/CPSETI64 and CPGETF/CPSETF).
    ** Their signatures do not carry the element width, so accepting a
    ** 1/2-byte integer or a 4-byte float here would silently use the
    ** wrong stride and corrupt neighbouring elements.  Leave those
    ** widths on the interpreter. */
    if (elem->kind == CT_INT && (elem->size == 4 || elem->size == 8))
      *kind = 1;
    else if (elem->kind == CT_FLOAT && elem->size == 8)
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


/* True for any FFI function cdata (callable), regardless of whether a J4
** scalar-call op can inline it.  The recorder uses this to remember the
** live function value instead of reading a result slot left by the
** preceding iteration. */
int luar_ffi_trace_is_function (TValue *fn) {
  Udata *u;
  CData *cd;
  if (fn == NULL || !ttisfulluserdata(fn))
    return 0;
  u = (Udata *)fn->value_.gc;
  cd = (CData *)getudatamem(u);
  return cd->magic == LUAR_FFI_MAGIC &&
         (cd->flags & (CDATA_CTYPE | CDATA_CALLBACK | CDATA_FUNC)) ==
           CDATA_FUNC;
}


/* True when `fn` is an FFI function cdata that no J4 scalar-call op can
** inline.  Recording such a CALL as a generic call exit would run the
** post-call continuation with the interpreter's cdata/aggregate result in
** a slot the continuation assumes is a Lua value (64-bit ints are the
** common case).  The recorder aborts the trace instead, leaving the whole
** loop on the interpreter. */
int luar_ffi_trace_call_unsupported (TValue *fn) {
  if (!luar_ffi_trace_is_function(fn))
    return 0;
  return !(luar_ffi_trace_ccall0_kind(fn) > 0 ||
           luar_ffi_trace_ccall1_kind(fn) > 0 ||
           luar_ffi_trace_ccall1di_kind(fn) > 0 ||
           luar_ffi_trace_ccall1dd_kind(fn) > 0 ||
           luar_ffi_trace_ccall1ff_kind(fn) > 0 ||
           luar_ffi_trace_ccall2dd_kind(fn) > 0 ||
           luar_ffi_trace_ccall2ff_kind(fn) > 0 ||
           luar_ffi_trace_ccall2di_kind(fn) > 0 ||
           luar_ffi_trace_ccall2id_kind(fn) > 0 ||
           luar_ffi_trace_ccall2fi_kind(fn) > 0 ||
           luar_ffi_trace_ccall2if_kind(fn) > 0 ||
           luar_ffi_trace_ccall2fd_kind(fn) > 0 ||
           luar_ffi_trace_ccall2df_kind(fn) > 0 ||
           luar_ffi_trace_ccall2ii_kind(fn) > 0 ||
           luar_ffi_trace_ccall3dd_kind(fn) > 0 ||
           luar_ffi_trace_ccall3ddi_kind(fn) > 0 ||
           luar_ffi_trace_ccall3did_kind(fn) > 0 ||
           luar_ffi_trace_ccall3idd_kind(fn) > 0);
}
