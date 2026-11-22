/*
** luar_trace.c -- tracing-IR construction, verification and exit snapshots.
*/
#include "lprefix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lobject.h"
#include "lstate.h"
#include "lopcodes.h"
#include "ltable.h"
#include "lstring.h"
#include "ltm.h"
#include "luar_trace.h"
#include "luar_jit.h"

void luar_trace_ir_init (LuaR_TraceIR *t, Proto *proto, int32_t startpc,
                         int32_t maxslots, int depth) {
  memset(t, 0, sizeof(*t));
  t->proto = proto;
  t->startpc = startpc;
  t->maxslots = maxslots;
  t->depth = depth;
}

int luar_trace_ir_emit (LuaR_TraceIR *t, const LuaR_IRIns *ins) {
  if (t->nins >= LUAR_IR_MAX_INS)
    return 0;
  t->ins[t->nins++] = *ins;
  return 1;
}

int luar_trace_ir_add_exit (LuaR_TraceIR *t, const LuaR_IRExitState *ex) {
  if (t->nexits >= LUAR_IR_MAX_EXITS || ex->nslots > LUAR_IR_MAX_SLOTS)
    return 0;
  t->exits[t->nexits++] = *ex;
  return 1;
}

static int valid_slot (const LuaR_TraceIR *t, int s) {
  return s >= 0 && s < t->maxslots && s < LUAR_IR_MAX_SLOTS;
}

int luar_trace_ir_verify (const LuaR_TraceIR *t, char *err, size_t errlen) {
  int i;
  if (t == NULL) {
    if (err != NULL && errlen > 0) snprintf(err, errlen, "null trace");
    return 0;
  }
  if (t->maxslots <= 0 || t->maxslots > LUAR_IR_MAX_SLOTS) {
    if (err != NULL && errlen > 0) snprintf(err, errlen, "bad slot count");
    return 0;
  }
  if (t->depth < 0 || t->depth > LUAR_IR_MAX_DEPTH) {
    if (err != NULL && errlen > 0) snprintf(err, errlen, "bad depth");
    return 0;
  }
  for (i = 0; i < t->nins; i++) {
    const LuaR_IRIns *in = &t->ins[i];
    switch (in->kind) {
      case LUAR_IR_KINT:
      case LUAR_IR_KFLOAT:
        break;
      case LUAR_IR_LOADI:
      case LUAR_IR_KBOOL:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad kbool");
          return 0;
        }
        break;
      case LUAR_IR_KSTR:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad kstr");
          return 0;
        }
        break;
      case LUAR_IR_LOADF:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad loadi");
          return 0;
        }
        break;
      case LUAR_IR_ADDI:
      case LUAR_IR_ADDK:
      case LUAR_IR_SUBK:
      case LUAR_IR_MULK:
      case LUAR_IR_DIVK:
      case LUAR_IR_MODK:
      case LUAR_IR_IDIVK:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad addi");
          return 0;
        }
        break;
      case LUAR_IR_SLOAD:
      case LUAR_IR_SSTORE:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad slot");
          return 0;
        }
        break;
      case LUAR_IR_ULOAD:
        if (!valid_slot(t, in->a) || in->ival == 0 ||
            in->nval < 0.0 || in->nval > 255.0 ||
            (double)(int)in->nval != in->nval) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad uload");
          return 0;
        }
        break;
      case LUAR_IR_GUARD:
        if (!valid_slot(t, in->a) ||
            in->b < LUAR_IR_TAG_INT ||
            in->b > LUAR_IR_TAG_USERDATA) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad guard");
          return 0;
        }
        break;
      case LUAR_IR_ADD:
      case LUAR_IR_SUB:
      case LUAR_IR_MUL:
      case LUAR_IR_DIV:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad arith");
          return 0;
        }
        break;
      case LUAR_IR_CALLEXIT:
        if (in->pc < 0) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad callexit");
          return 0;
        }
        break;
      case LUAR_IR_CGUARD:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad cguard");
          return 0;
        }
        break;
      case LUAR_IR_TGETF:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tgetf");
          return 0;
        }
        break;
      case LUAR_IR_ITOF:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad itof");
          return 0;
        }
        break;
      case LUAR_IR_FADDI:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad faddi");
          return 0;
        }
        break;
      case LUAR_IR_FMULK:
      case LUAR_IR_FADD:
      case LUAR_IR_FSUB:
      case LUAR_IR_FDIVK:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad float op");
          return 0;
        }
        break;
      case LUAR_IR_CGETI:
      case LUAR_IR_CGETF:
      case LUAR_IR_CSETI:
      case LUAR_IR_CSETF:
      case LUAR_IR_CGETFIELD:
      case LUAR_IR_CSETFIELD:
      case LUAR_IR_CSETFIELDK:
      case LUAR_IR_CPGETI:
      case LUAR_IR_CPGETF:
      case LUAR_IR_CPSETI:
      case LUAR_IR_CPSETF:
      case LUAR_IR_CPGETI64:
      case LUAR_IR_CPSETI64:
      case LUAR_IR_CCALL0I:
      case LUAR_IR_CCALL0D:
        break;
      case LUAR_IR_CCALL1DI:
      case LUAR_IR_CCALL1DD:
      case LUAR_IR_CCALL1FF:
      case LUAR_IR_CCALL1I:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad ccall1i");
          return 0;
        }
        break;
      case LUAR_IR_TSETDYN_KB:
      case LUAR_IR_TSETDYN_KI:
      case LUAR_IR_TSETDYN_KS:
      case LUAR_IR_TSETDYN_KF:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetdynkf");
          return 0;
        }
        break;
      case LUAR_IR_TSETDYN:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetdyn");
          return 0;
        }
        break;
      case LUAR_IR_TSETUPKN:
        break;
      case LUAR_IR_TSETUPKV:
      case LUAR_IR_TSETUPKT:
        if (!valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetupt");
          return 0;
        }
        break;
      case LUAR_IR_TSETUPKB:
        break;
      case LUAR_IR_TSETUPKS:
        break;
      case LUAR_IR_TSETUPKI:
        break;
      case LUAR_IR_TSETUPKF:
        break;
      case LUAR_IR_TSETKDB:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetkdb");
          return 0;
        }
        break;
      case LUAR_IR_TSETKB:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetkb");
          return 0;
        }
        break;
      case LUAR_IR_TSETKT:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetkt");
          return 0;
        }
        break;
      case LUAR_IR_TSETKS:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetks");
          return 0;
        }
        break;
      case LUAR_IR_TSETKKS:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetkks");
          return 0;
        }
        break;
      case LUAR_IR_TSETKKF:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetkkf");
          return 0;
        }
        break;
      case LUAR_IR_TSETKF:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetkf");
          return 0;
        }
        break;
      case LUAR_IR_TSETKK:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetkk");
          return 0;
        }
        break;
      case LUAR_IR_TLEN:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tlen");
          return 0;
        }
        break;
      case LUAR_IR_STRLEN:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad strlen");
          return 0;
        }
        break;
      case LUAR_IR_CONCATN:
        if (in->ival < 2 || in->ival > 32 ||
            in->a + in->ival > t->maxslots) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad concatn");
          return 0;
        }
        break;
      case LUAR_IR_CONCAT3:
        if (in->a + 2 >= t->maxslots) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad concat3");
          return 0;
        }
        break;
      case LUAR_IR_CONCAT2:
        if (in->a + 1 >= t->maxslots) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad concat2");
          return 0;
        }
        break;
      case LUAR_IR_TGETKT:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tgetkt");
          return 0;
        }
        break;
      case LUAR_IR_TGETKB:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tgetkb");
          return 0;
        }
        break;
      case LUAR_IR_TGETKS:
      case LUAR_IR_TGETKF:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tgetkf");
          return 0;
        }
        break;
      case LUAR_IR_TGETK:
      case LUAR_IR_TSETK:
        if (!valid_slot(t, in->a) ||
            (in->kind == LUAR_IR_TSETK && !valid_slot(t, in->b))) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tsetk");
          return 0;
        }
        break;
      case LUAR_IR_TGETI:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tgeti");
          return 0;
        }
        break;
      case LUAR_IR_TSETIF:
      case LUAR_IR_TSETIS:
      case LUAR_IR_TSETIB:
      case LUAR_IR_TSETIT:
      case LUAR_IR_TSETI:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tseti");
          return 0;
        }
        break;
      case LUAR_IR_TNEW:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tnew");
          return 0;
        }
        break;
      case LUAR_IR_CCALL2II:
      case LUAR_IR_CCALL2DI:
      case LUAR_IR_CCALL2ID:
      case LUAR_IR_CCALL2FI:
      case LUAR_IR_CCALL2IF:
      case LUAR_IR_CCALL2FD:
      case LUAR_IR_CCALL2DF:
      case LUAR_IR_CCALL2FF:
      case LUAR_IR_CCALL2DD:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c) || !valid_slot(t, (int)in->ival)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad ccall2dd");
          return 0;
        }
        break;
      case LUAR_IR_CCALL3IDD:
      case LUAR_IR_CCALL3DID:
      case LUAR_IR_CCALL3DDI:
      case LUAR_IR_CCALL3DD: {
        int a2 = (int)(in->ival & 0xff);
        int a3 = (int)((in->ival >> 8) & 0xff);
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c) || !valid_slot(t, a2) ||
            !valid_slot(t, a3)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad ccall3dd");
          return 0;
        }
        break;
      }
      case LUAR_IR_TGET:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tget");
          return 0;
        }
        break;
      case LUAR_IR_TSET:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->b) ||
            !valid_slot(t, in->c)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad tset");
          return 0;
        }
        break;
      case LUAR_IR_FORLOOP:
        if (!valid_slot(t, in->a) || !valid_slot(t, in->a + 1) ||
            !valid_slot(t, in->a + 2)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad forloop");
          return 0;
        }
        break;
      case LUAR_IR_EXIT:
        if (in->a >= t->nexits) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad exit");
          return 0;
        }
        break;
      case LUAR_IR_RET:
        break;
      default:
        if (err != NULL && errlen > 0) snprintf(err, errlen, "bad opcode");
        return 0;
    }
  }
  for (i = 0; i < t->nexits; i++) {
    const LuaR_IRExitState *ex = &t->exits[i];
    if (ex->nslots > t->maxslots || ex->pc < 0) {
      if (err != NULL && errlen > 0) snprintf(err, errlen, "bad exit state");
      return 0;
    }
  }
  return 1;
}

int luar_trace_ir_snapshot (LuaR_IRExitState *ex, int32_t pc, TValue *base,
                            int nslots) {
  int i;
  if (ex == NULL || base == NULL || nslots < 0 || nslots > LUAR_IR_MAX_SLOTS)
    return 0;
  memset(ex, 0, sizeof(*ex));
  ex->pc = pc;
  ex->nslots = (uint8_t)nslots;
  for (i = 0; i < nslots; i++) {
    ex->tags[i] = ttypetag(&base[i]);
    memcpy(&ex->bits[i], &base[i].value_, sizeof(uint64_t));
  }
  return 1;
}

int luar_trace_ir_restore (const LuaR_IRExitState *ex, TValue *base) {
  int i;
  if (ex == NULL || base == NULL || ex->nslots > LUAR_IR_MAX_SLOTS)
    return 0;
  for (i = 0; i < ex->nslots; i++) {
    memcpy(&base[i].value_, &ex->bits[i], sizeof(uint64_t));
    base[i].tt_ = (lu_byte)ex->tags[i];
  }
  return 1;
}



static int ir_number (TValue *v, double *out);
int64_t luar_trace_fsub (TValue *a, TValue *b, TValue *c) {
  double x, y;
  if (!ir_number(b, &x) || !ir_number(c, &y))
    return 0;
  if (ttisinteger(b) && ttisinteger(c))
    return 0;   /* integer subtraction keeps integer type */
  setfltvalue(a, x - y);
  return 1;
}

int64_t luar_trace_fdivk (TValue *a, TValue *b, double k) {
  double x;
  if (!ir_number(b, &x))
    return 0;
  setfltvalue(a, x / k);
  return 1;
}


static int ir_number (TValue *v, double *out) {
  if (ttisinteger(v)) { *out = (double)ivalue(v); return 1; }
  if (ttisfloat(v)) { *out = fltvalue(v); return 1; }
  return 0;
}

int luar_trace_table_elem_tag (TValue *tv, lua_Integer key) {
  Table *t;
  if (tv == NULL || !ttistable(tv))
    return 0;
  t = (Table *)tv->value_.gc;
  if (t->metatable != NULL || key < 1 || (lua_Unsigned)key > t->asize)
    return 0;
  switch (*getArrTag(t, (unsigned)(key - 1))) {
    case LUA_VNUMINT: return LUAR_IR_TAG_INT;
    case LUA_VNUMFLT: return LUAR_IR_TAG_FLOAT;
    default: return 0;
  }
}


int64_t luar_trace_fadd (TValue *a, TValue *b, TValue *c) {
  double x, y;
  if (!ir_number(b, &x) || !ir_number(c, &y))
    return 0;
  setfltvalue(a, x + y);
  return 1;
}

int64_t luar_trace_fmulk (TValue *a, TValue *b, double k) {
  double x;
  if (!ir_number(b, &x))
    return 0;
  setfltvalue(a, x * k);
  return 1;
}

int64_t luar_trace_tgetf_generic (TValue *tv, lua_Integer key,
                                   double *out) {
  TValue k, res;
  Table *t;
  if (tv == NULL || !ttistable(tv))
    return 0;
  t = (Table *)tv->value_.gc;
  if (t->metatable != NULL)
    return 0;
  setivalue(&k, key);
  if (luaH_get(t, &k, &res) != LUA_VNUMFLT)
    return 0;
  *out = fltvalue(&res);
  return 1;
}


int64_t luar_trace_tget_generic (TValue *tv, lua_Integer key,
                                  int64_t *out) {
  TValue k, res;
  Table *t;
  if (tv == NULL || !ttistable(tv))
    return 0;
  t = (Table *)tv->value_.gc;
  setivalue(&k, key);
  if (luaH_get(t, &k, &res) != LUA_VNUMINT)
    return 0;
  *out = ivalue(&res);
  return 1;
}


int64_t luar_trace_uload (lua_State *L, TValue *out, LClosure *expected,
                          int idx) {
  LClosure *cl;
  UpVal *uv;
  if (L == NULL || out == NULL || expected == NULL || idx < 0)
    return 0;
  if (L->ci == NULL || !ttisLclosure(s2v(L->ci->func.p)))
    return 0;
  cl = clLvalue(s2v(L->ci->func.p));
  if (cl != expected || idx >= (int)cl->nupvalues)
    return 0;
  uv = cl->upvals[idx];
  if (uv == NULL || uv->v.p == NULL)
    return 0;
  *out = *uv->v.p;
  return 1;
}


int64_t luar_trace_tnew (lua_State *L, TValue *out, int asize, int hsize) {
  Table *t;
  if (L == NULL || out == NULL || asize < 0 || hsize < 0)
    return 0;
  t = luaH_new(L);
  sethvalue(L, out, t);   /* anchor before resize can trigger GC */
  if (G(L)->luar_jit != NULL)
    G(L)->luar_jit->ir_alloc_tables++;
  if (asize > 0 || hsize > 0)
    luaH_resize(L, t, (unsigned)asize, (unsigned)hsize);
  return 1;
}


int64_t luar_trace_tgetkf (TValue *tv, TString *key, TValue *out) {
  Table *t;
  TValue v;
  if (tv == NULL || !ttistable(tv) || key == NULL || out == NULL)
    return 0;
  t = (Table *)tv->value_.gc;
  luaH_getstr(t, key, &v);
  if (!ttisfloat(&v))
    return 0;
  setfltvalue(out, fltvalue(&v));
  return 1;
}


static int concat_part (TValue *v, const char **p, char *buf, size_t *len);

int64_t luar_trace_concatn (TValue *base, int a, int n, lua_State *L) {
  const char *p[32];
  char nb[32][LUA_N2SBUFFSZ];
  size_t l[32], len, off;
  char *buf;
  TString *r;
  int i;
  if (base == NULL || L == NULL || a < 0 || n < 2 || n > 32)
    return 0;
  for (i = 0; i < n; i++)
    if (!concat_part(&base[a + i], &p[i], nb[i], &l[i]))
      return 0;
  len = 0;
  for (i = 0; i < n; i++) len += l[i];
  buf = (char *)malloc(len > 0 ? len : 1);
  if (buf == NULL)
    return 0;
  off = 0;
  for (i = 0; i < n; i++) {
    memcpy(buf + off, p[i], l[i]);
    off += l[i];
  }
  r = luaS_newlstr(L, buf, len);
  free(buf);
  setsvalue(L, &base[a], r);
  return 1;
}


static int concat_part (TValue *v, const char **p, char *buf, size_t *len) {
  if (ttisstring(v)) {
    *p = getstr(tsvalue(v));
    *len = tsslen(tsvalue(v));
    return 1;
  }
  if (ttisnumber(v)) {
    *len = luaO_tostringbuff(v, buf);
    *p = buf;
    return 1;
  }
  return 0;
}


int64_t luar_trace_concat3 (TValue *base, int a, lua_State *L) {
  const char *p[3];
  char nb[3][LUA_N2SBUFFSZ];
  size_t l[3], len, off;
  char *buf;
  TString *r;
  int i;
  if (base == NULL || L == NULL || a < 0)
    return 0;
  for (i = 0; i < 3; i++)
    if (!concat_part(&base[a + i], &p[i], nb[i], &l[i]))
      return 0;
  len = l[0] + l[1] + l[2];
  buf = (char *)malloc(len > 0 ? len : 1);
  if (buf == NULL)
    return 0;
  off = 0;
  for (i = 0; i < 3; i++) {
    memcpy(buf + off, p[i], l[i]);
    off += l[i];
  }
  r = luaS_newlstr(L, buf, len);
  free(buf);
  setsvalue(L, &base[a], r);
  return 1;
}


int64_t luar_trace_tlen (TValue *v, TValue *out, lua_State *L) {
  Table *t;
  if (v == NULL || out == NULL || !ttistable(v) || L == NULL)
    return 0;
  t = (Table *)v->value_.gc;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_LEN))
    return 0;
  setivalue(out, (lua_Integer)luaH_getn(L, t));
  return 1;
}


int64_t luar_trace_kbool (TValue *out, int b) {
  if (out == NULL)
    return 0;
  settt_(out, b != 0 ? LUA_VTRUE : LUA_VFALSE);
  return 1;
}


int64_t luar_trace_kstr (TValue *out, TString *s, lua_State *L) {
  if (out == NULL || s == NULL || L == NULL)
    return 0;
  setsvalue(L, out, s);
  return 1;
}


int64_t luar_trace_faddk (TValue *out, TValue *src, double k) {
  double x;
  if (out == NULL || src == NULL)
    return 0;
  if (ttisfloat(src)) x = fltvalue(src);
  else if (ttisinteger(src)) x = (double)ivalue(src);
  else return 0;
  setfltvalue(out, x + k);
  return 1;
}


int64_t luar_trace_strlen (TValue *v, TValue *out) {
  if (v == NULL || out == NULL || !ttisstring(v))
    return 0;
  setivalue(out, (lua_Integer)tsslen(tsvalue(v)));
  return 1;
}


int64_t luar_trace_concat2 (TValue *base, int a, lua_State *L) {
  TString *r;
  const char *p1, *p2;
  char n1[LUA_N2SBUFFSZ], n2[LUA_N2SBUFFSZ];
  size_t l1, l2, len;
  char *buf;
  if (base == NULL || L == NULL || a < 0)
    return 0;
  if (ttisstring(&base[a])) {
    p1 = getstr(tsvalue(&base[a]));
    l1 = tsslen(tsvalue(&base[a]));
  }
  else if (ttisnumber(&base[a])) {
    l1 = luaO_tostringbuff(&base[a], n1);
    p1 = n1;
  }
  else
    return 0;
  if (ttisstring(&base[a + 1])) {
    p2 = getstr(tsvalue(&base[a + 1]));
    l2 = tsslen(tsvalue(&base[a + 1]));
  }
  else if (ttisnumber(&base[a + 1])) {
    l2 = luaO_tostringbuff(&base[a + 1], n2);
    p2 = n2;
  }
  else
    return 0;
  len = l1 + l2;
  buf = (char *)malloc(len > 0 ? len : 1);
  if (buf == NULL)
    return 0;
  memcpy(buf, p1, l1);
  memcpy(buf + l1, p2, l2);
  r = luaS_newlstr(L, buf, len);
  free(buf);
  setsvalue(L, &base[a], r);
  return 1;
}


int64_t luar_trace_tgetkt (TValue *tv, TString *key, TValue *out) {
  Table *t; TValue v;
  if (tv == NULL || !ttistable(tv) || key == NULL || out == NULL)
    return 0;
  t = (Table *)tv->value_.gc;
  luaH_getstr(t, key, &v);
  if (!ttistable(&v))
    return 0;
  *out = v;
  return 1;
}


int64_t luar_trace_tgetkb (TValue *tv, TString *key, TValue *out) {
  Table *t; TValue v;
  if (tv == NULL || !ttistable(tv) || key == NULL || out == NULL)
    return 0;
  t = (Table *)tv->value_.gc;
  luaH_getstr(t, key, &v);
  if (!ttisboolean(&v))
    return 0;
  *out = v;
  return 1;
}


int64_t luar_trace_tgetks (TValue *tv, TString *key, TValue *out) {
  Table *t;
  TValue v;
  if (tv == NULL || !ttistable(tv) || key == NULL || out == NULL)
    return 0;
  t = (Table *)tv->value_.gc;
  luaH_getstr(t, key, &v);
  if (!ttisstring(&v))
    return 0;
  *out = v;
  return 1;
}


TValue *luar_trace_tsetkk (TValue *tv, TString *key, lua_Integer val,
                           lua_State *L) {
  Table *t;
  TValue k, v;
  if (tv == NULL || !ttistable(tv) || key == NULL || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  setivalue(&v, val);
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetkks (TValue *tv, TString *key, uint64_t bits,
                            lua_State *L) {
  Table *t;
  TValue k, v;
  TString *vs;
  if (tv == NULL || !ttistable(tv) || key == NULL || L == NULL)
    return NULL;
  memcpy(&vs, &bits, sizeof(vs));
  if (vs == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  setsvalue(L, &v, vs);
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetkkf (TValue *tv, TString *key, uint64_t bits,
                            lua_State *L) {
  Table *t;
  TValue k, v;
  double d;
  if (tv == NULL || !ttistable(tv) || key == NULL || L == NULL)
    return NULL;
  memcpy(&d, &bits, sizeof(d));
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  setfltvalue(&v, d);
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetdynkb (TValue *tv, TValue *key, int b,
                                  lua_State *L) {
  Table *t; TValue k, val;
  if (tv == NULL || !ttistable(tv) || key == NULL || L == NULL)
    return NULL;
  if (!ttisstring(key) && !ttisinteger(key))
    return NULL;
  t = (Table *)tv->value_.gc;
  if (t->metatable != NULL)
    return NULL;
  k = *key;
  settt_(&val, b != 0 ? LUA_VTRUE : LUA_VFALSE);
  luaH_set(L, t, &k, &val);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetdynki (TValue *tv, TValue *key, int64_t v,
                                  lua_State *L) {
  Table *t; TValue k, val;
  if (tv == NULL || !ttistable(tv) || key == NULL || L == NULL)
    return NULL;
  if (!ttisstring(key) && !ttisinteger(key))
    return NULL;
  t = (Table *)tv->value_.gc;
  if (t->metatable != NULL)
    return NULL;
  k = *key;
  setivalue(&val, (lua_Integer)v);
  luaH_set(L, t, &k, &val);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetdynks (TValue *tv, TValue *key, TString *s,
                                  lua_State *L) {
  Table *t; TValue k, val;
  if (tv == NULL || !ttistable(tv) || key == NULL || s == NULL || L == NULL)
    return NULL;
  if (!ttisstring(key) && !ttisinteger(key))
    return NULL;
  t = (Table *)tv->value_.gc;
  if (t->metatable != NULL)
    return NULL;
  k = *key;
  setsvalue(L, &val, s);
  luaH_set(L, t, &k, &val);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetdynkf (TValue *tv, TValue *key, double v,
                                 lua_State *L) {
  Table *t; TValue k, val;
  if (tv == NULL || !ttistable(tv) || key == NULL || L == NULL)
    return NULL;
  if (!ttisstring(key) && !ttisinteger(key))
    return NULL;
  t = (Table *)tv->value_.gc;
  if (t->metatable != NULL)
    return NULL;
  k = *key;
  setfltvalue(&val, v);
  luaH_set(L, t, &k, &val);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetdyn (TValue *tv, TValue *key, TValue *val,
                            lua_State *L) {
  Table *t;
  TValue k, v;
  if (tv == NULL || !ttistable(tv) || key == NULL || val == NULL ||
      ttisnil(val) || L == NULL)
    return NULL;
  if (!ttisstring(key) && !ttisinteger(key))
    return NULL;
  t = (Table *)tv->value_.gc;
  if (t->metatable != NULL)
    return NULL;
  k = *key;
  v = *val;
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


int64_t luar_trace_tsetupki (int idx, TString *key, int64_t v,
                                lua_State *L) {
  LClosure *cl;
  TValue *uv;
  Table *t;
  TValue k, val;
  if (L == NULL || key == NULL || idx < 0)
    return 0;
  if (!ttisLclosure(s2v(L->ci->func.p)))
    return 0;
  cl = clLvalue(s2v(L->ci->func.p));
  if (idx >= (int)cl->nupvalues || cl->upvals[idx] == NULL ||
      cl->upvals[idx]->v.p == NULL)
    return 0;
  uv = cl->upvals[idx]->v.p;
  if (!ttistable(uv))
    return 0;
  t = (Table *)uv->value_.gc;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return 0;
  setsvalue(L, &k, key);
  setivalue(&val, (lua_Integer)v);
  luaH_set(L, t, &k, &val);
  return 1;
}


int64_t luar_trace_tsetupks (int idx, TString *key, TString *v,
                                lua_State *L) {
  LClosure *cl; TValue *uv; Table *t; TValue k, val;
  if (L == NULL || key == NULL || v == NULL || idx < 0) return 0;
  if (!ttisLclosure(s2v(L->ci->func.p))) return 0;
  cl = clLvalue(s2v(L->ci->func.p));
  if (idx >= (int)cl->nupvalues || cl->upvals[idx] == NULL ||
      cl->upvals[idx]->v.p == NULL) return 0;
  uv = cl->upvals[idx]->v.p;
  if (!ttistable(uv)) return 0;
  t = (Table *)uv->value_.gc;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return 0;
  setsvalue(L, &k, key);
  setsvalue(L, &val, v);
  luaH_set(L, t, &k, &val);
  return 1;
}


int64_t luar_trace_tsetupkb (int idx, TString *key, int b,
                                lua_State *L) {
  LClosure *cl; TValue *uv; Table *t; TValue k, val;
  if (L == NULL || key == NULL || idx < 0) return 0;
  if (!ttisLclosure(s2v(L->ci->func.p))) return 0;
  cl = clLvalue(s2v(L->ci->func.p));
  if (idx >= (int)cl->nupvalues || cl->upvals[idx] == NULL ||
      cl->upvals[idx]->v.p == NULL) return 0;
  uv = cl->upvals[idx]->v.p;
  if (!ttistable(uv)) return 0;
  t = (Table *)uv->value_.gc;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return 0;
  setsvalue(L, &k, key);
  settt_(&val, b != 0 ? LUA_VTRUE : LUA_VFALSE);
  luaH_set(L, t, &k, &val);
  return 1;
}


int64_t luar_trace_tsetupv (int idx, TString *key, TValue *val,
                                lua_State *L) {
  LClosure *cl; TValue *uv; Table *t; TValue k, v;
  if (L == NULL || key == NULL || val == NULL || ttisnil(val) || idx < 0)
    return 0;
  if (!ttisLclosure(s2v(L->ci->func.p))) return 0;
  cl = clLvalue(s2v(L->ci->func.p));
  if (idx >= (int)cl->nupvalues || cl->upvals[idx] == NULL ||
      cl->upvals[idx]->v.p == NULL) return 0;
  uv = cl->upvals[idx]->v.p;
  if (!ttistable(uv)) return 0;
  t = (Table *)uv->value_.gc;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return 0;
  setsvalue(L, &k, key);
  v = *val;
  luaH_set(L, t, &k, &v);
  return 1;
}


int64_t luar_trace_tsetupt (int idx, TString *key, TValue *val,
                                lua_State *L) {
  LClosure *cl; TValue *uv; Table *t; TValue k, v;
  if (L == NULL || key == NULL || val == NULL || !ttistable(val) || idx < 0)
    return 0;
  if (!ttisLclosure(s2v(L->ci->func.p))) return 0;
  cl = clLvalue(s2v(L->ci->func.p));
  if (idx >= (int)cl->nupvalues || cl->upvals[idx] == NULL ||
      cl->upvals[idx]->v.p == NULL) return 0;
  uv = cl->upvals[idx]->v.p;
  if (!ttistable(uv)) return 0;
  t = (Table *)uv->value_.gc;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return 0;
  setsvalue(L, &k, key);
  v = *val;
  luaH_set(L, t, &k, &v);
  return 1;
}


int64_t luar_trace_tsetupkn (int idx, TString *key, lua_State *L) {
  LClosure *cl; TValue *uv; Table *t; TValue k, v;
  if (L == NULL || key == NULL || idx < 0) return 0;
  if (!ttisLclosure(s2v(L->ci->func.p))) return 0;
  cl = clLvalue(s2v(L->ci->func.p));
  if (idx >= (int)cl->nupvalues || cl->upvals[idx] == NULL ||
      cl->upvals[idx]->v.p == NULL) return 0;
  uv = cl->upvals[idx]->v.p;
  if (!ttistable(uv)) return 0;
  t = (Table *)uv->value_.gc;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return 0;
  setsvalue(L, &k, key);
  setnilvalue(&v);
  luaH_set(L, t, &k, &v);
  return 1;
}


int64_t luar_trace_itof (TValue *out, TValue *in) {
  if (out == NULL || in == NULL)
    return 0;
  if (ttisinteger(in)) {
    setfltvalue(out, (double)ivalue(in));
    return 1;
  }
  if (ttisfloat(in)) {
    *out = *in;
    return 1;
  }
  return 0;
}


int64_t luar_trace_tsetupkf (int idx, TString *key, double v,
                                lua_State *L) {
  LClosure *cl;
  TValue *uv;
  Table *t;
  TValue k, val;
  if (L == NULL || key == NULL || idx < 0)
    return 0;
  if (!ttisLclosure(s2v(L->ci->func.p)))
    return 0;
  cl = clLvalue(s2v(L->ci->func.p));
  if (idx >= (int)cl->nupvalues || cl->upvals[idx] == NULL ||
      cl->upvals[idx]->v.p == NULL)
    return 0;
  uv = cl->upvals[idx]->v.p;
  if (!ttistable(uv))
    return 0;
  t = (Table *)uv->value_.gc;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return 0;
  setsvalue(L, &k, key);
  setfltvalue(&val, v);
  luaH_set(L, t, &k, &val);
  return 1;
}


TValue *luar_trace_tsetkdb (TValue *tv, TString *key, TValue *val,
                            lua_State *L) {
  Table *t;
  TValue k, v;
  if (tv == NULL || !ttistable(tv) || key == NULL || val == NULL ||
      !ttisboolean(val) || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  v = *val;
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetkb (TValue *tv, TString *key, int b, lua_State *L) {
  Table *t;
  TValue k, v;
  if (tv == NULL || !ttistable(tv) || key == NULL || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  settt_(&v, b != 0 ? LUA_VTRUE : LUA_VFALSE);
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetkt (TValue *tv, TString *key, TValue *val,
                           lua_State *L) {
  Table *t;
  TValue k, v;
  if (tv == NULL || !ttistable(tv) || key == NULL || val == NULL ||
      !ttistable(val) || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  v = *val;
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetks (TValue *tv, TString *key, TValue *val,
                           lua_State *L) {
  Table *t;
  TValue k, v;
  if (tv == NULL || !ttistable(tv) || key == NULL || val == NULL ||
      !ttisstring(val) || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  v = *val;
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetkf (TValue *tv, TString *key, TValue *val,
                           lua_State *L) {
  Table *t;
  TValue k, v;
  if (tv == NULL || !ttistable(tv) || key == NULL || val == NULL ||
      !ttisfloat(val) || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  v = *val;
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetib (TValue *tv, lua_Integer key, TValue *val,
                           lua_State *L) {
  Table *t;
  TValue v;
  if (tv == NULL || !ttistable(tv) || val == NULL || !ttisboolean(val) ||
      key < 1 || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  v = *val;
  luaH_setint(L, t, key, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetit (TValue *tv, lua_Integer key, TValue *val,
                           lua_State *L) {
  Table *t;
  TValue v;
  if (tv == NULL || !ttistable(tv) || val == NULL || !ttistable(val) ||
      key < 1 || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  v = *val;
  luaH_setint(L, t, key, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetif (TValue *tv, lua_Integer key, TValue *val,
                           lua_State *L) {
  Table *t;
  TValue v;
  if (tv == NULL || !ttistable(tv) || val == NULL || !ttisfloat(val) ||
      key < 1 || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  v = *val;
  luaH_setint(L, t, key, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tsetis (TValue *tv, lua_Integer key, TValue *val,
                           lua_State *L) {
  Table *t;
  TValue v;
  if (tv == NULL || !ttistable(tv) || val == NULL || !ttisstring(val) ||
      key < 1 || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  v = *val;
  luaH_setint(L, t, key, &v);
  return (TValue *)(L->ci->func.p + 1);
}


TValue *luar_trace_tseti (TValue *tv, lua_Integer key, TValue *val,
                          lua_State *L) {
  return luar_trace_tset_insert(tv, key, val, L);
}


TValue *luar_trace_tset_insert (TValue *tv, lua_Integer key, TValue *val,
                                lua_State *L) {
  Table *t;
  TValue old, v;
  lu_byte tag;
  if (tv == NULL || !ttistable(tv) || val == NULL || !ttisinteger(val) ||
      key < 1 || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  if ((lua_Unsigned)key <= t->asize) {
    if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX) &&
        tagisempty(*getArrTag(t, (unsigned)(key - 1))))
      return NULL;
    *getArrTag(t, (unsigned)(key - 1)) = LUA_VNUMINT;
    getArrVal(t, (unsigned)(key - 1))->i = ivalue(val);
    return (TValue *)(L->ci->func.p + 1);
  }
  tag = luaH_getint(t, key, &old);
  if (!tagisempty(tag) && !ttisnil(&old)) {
    v = *val;
    luaH_setint(L, t, key, &v);
    return (TValue *)(L->ci->func.p + 1);
  }
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return NULL;   /* absent key: __newindex owns the write */
  v = *val;
  luaH_setint(L, t, key, &v);   /* may grow/rehash and move the stack */
  return (TValue *)(L->ci->func.p + 1);
}


int64_t luar_trace_tgeti (TValue *tv, lua_Integer key, TValue *out) {
  Table *t;
  TValue v;
  if (tv == NULL || !ttistable(tv) || out == NULL || key < 1)
    return 0;
  t = (Table *)tv->value_.gc;
  if ((lua_Unsigned)key <= t->asize &&
      *getArrTag(t, (unsigned)(key - 1)) == LUA_VNUMINT) {
    setivalue(out, getArrVal(t, (unsigned)(key - 1))->i);
    return 1;
  }
  (void)luaH_getint(t, key, &v);
  if (!ttisinteger(&v))
    return 0;
  setivalue(out, ivalue(&v));
  return 1;
}


TValue *luar_trace_tsetk (TValue *tv, TString *key, TValue *val,
                          lua_State *L) {
  Table *t;
  TValue k, v;
  if (tv == NULL || !ttistable(tv) || key == NULL || val == NULL ||
      !ttisinteger(val) || L == NULL)
    return NULL;
  t = (Table *)tv->value_.gc;
  setsvalue(L, &k, key);
  v = *val;
  luaH_set(L, t, &k, &v);
  return (TValue *)(L->ci->func.p + 1);
}


int64_t luar_trace_tgetk (TValue *tv, TString *key, TValue *out) {
  Table *t;
  TValue v;
  if (tv == NULL || !ttistable(tv) || key == NULL || out == NULL)
    return 0;
  t = (Table *)tv->value_.gc;
  luaH_getstr(t, key, &v);
  if (!ttisinteger(&v))
    return 0;
  setivalue(out, ivalue(&v));
  return 1;
}


int64_t luar_trace_tset_hash (TValue *tv, lua_Integer key, TValue *val) {
  Table *t;
  TValue old;
  lu_byte tag;
  if (tv == NULL || !ttistable(tv) || val == NULL || !ttisinteger(val))
    return 0;
  t = (Table *)tv->value_.gc;
  if (key < 1)
    return 0;
  if ((lua_Unsigned)key <= t->asize)
    return luar_trace_tset_int(tv, key, val);
  tag = luaH_getint(t, key, &old);
  if (tagisempty(tag) || ttisnil(&old))
    return 0;   /* absent cells may grow/rehash: interpreter handles */
  luaH_setint(NULL, t, key, val);   /* existing cell, no allocation */
  return 1;
}


int64_t luar_trace_tset_int (TValue *tv, lua_Integer key, TValue *val) {
  Table *t;
  if (tv == NULL || !ttistable(tv) || val == NULL || !ttisinteger(val))
    return 0;
  t = (Table *)tv->value_.gc;
  if (key < 1 || (lua_Unsigned)key > t->asize)
    return 0;
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX) &&
      tagisempty(*getArrTag(t, (unsigned)(key - 1))))
    return 0;
  *getArrTag(t, (unsigned)(key - 1)) = LUA_VNUMINT;
  getArrVal(t, (unsigned)(key - 1))->i = ivalue(val);
  return 1;
}


int64_t luar_trace_tget_int (TValue *tv, lua_Integer key, int64_t *out) {
  Table *t;
  if (tv == NULL || !ttistable(tv))
    return 0;
  t = (Table *)tv->value_.gc;
  if (key < 1 || (lua_Unsigned)key > t->asize)
    return 0;
  if (*getArrTag(t, (unsigned)(key - 1)) != LUA_VNUMINT)
    return 0;
  *out = getArrVal(t, (unsigned)(key - 1))->i;
  return 1;
}


static int record_loop (Proto *p, int start, int backedge, TValue *base,
                        lua_State *L, LuaR_TraceIR *t, int partial,
                        int *callpc, int suffix);

int luar_trace_record_loop (Proto *p, int start, int backedge, TValue *base,
                            lua_State *L, LuaR_TraceIR *t) {
  return record_loop(p, start, backedge, base, L, t, 0, NULL, 0);
}

int luar_trace_record_call_prefix (Proto *p, int start, int backedge,
                                   TValue *base, lua_State *L,
                                   LuaR_TraceIR *t, int *callpc) {
  return record_loop(p, start, backedge, base, L, t, 1, callpc, 0);
}

int luar_trace_record_suffix (Proto *p, int start, int backedge,
                              TValue *base, lua_State *L, LuaR_TraceIR *t) {
  return record_loop(p, start, backedge, base, L, t, 0, NULL, 1);
}


static int abstract_tag (const TValue *v) {
  if (ttisinteger(v)) return LUAR_IR_TAG_INT;
  if (ttisfloat(v)) return LUAR_IR_TAG_FLOAT;
  if (ttistable(v)) return LUAR_IR_TAG_TABLE;
  if (ttisstring(v) && tsslen(tsvalue(v)) <= LUAI_MAXSHORTLEN)
    return LUAR_IR_TAG_STRING;
  return LUAR_IR_TAG_OTHER;
}


int luar_trace_ir_guards_match (const LuaR_TraceIR *t, TValue *base) {
  int i;
  if (t == NULL || base == NULL)
    return 0;
  for (i = 0; i < t->nins; i++) {
    const LuaR_IRIns *in = &t->ins[i];
    if (in->kind == LUAR_IR_GUARD &&
        abstract_tag(&base[in->a]) != in->b)
      return 0;
  }
  return 1;
}


int luar_trace_ir_analyze (LuaR_TraceIR *t, char *err, size_t errlen) {
  uint8_t kind[LUAR_IR_MAX_SLOTS];
  int i;
  if (t == NULL || !luar_trace_ir_verify(t, err, errlen))
    return 0;
  memset(kind, 0, sizeof(kind));
  for (i = 0; i < t->nins; i++) {
    const LuaR_IRIns *in = &t->ins[i];
    switch (in->kind) {
      case LUAR_IR_KINT:
      case LUAR_IR_LOADI:
        kind[in->a] = 1;
        break;
      case LUAR_IR_LOADF:
        kind[in->a] = 2;
        break;
      case LUAR_IR_SLOAD:
        kind[in->a] = kind[in->b];
        break;
      case LUAR_IR_ULOAD:
        kind[in->a] = 0;
        break;
      case LUAR_IR_ADDI:
      case LUAR_IR_ADDK:
      case LUAR_IR_SUBK:
      case LUAR_IR_MULK:
      case LUAR_IR_DIVK:
      case LUAR_IR_MODK:
      case LUAR_IR_IDIVK:
      case LUAR_IR_ADD:
      case LUAR_IR_SUB:
      case LUAR_IR_MUL:
      case LUAR_IR_DIV:
        kind[in->a] = 1;
        break;
      case LUAR_IR_TGET:
      case LUAR_IR_TSET:
        kind[in->a] = 0;
        break;
      case LUAR_IR_TGETF:
      case LUAR_IR_FMULK:
      case LUAR_IR_FADD:
      case LUAR_IR_FSUB:
      case LUAR_IR_FDIVK:
        kind[in->a] = 2;
        break;
      case LUAR_IR_CCALL0D:
        kind[in->a] = 2;
        break;
      case LUAR_IR_CCALL1I:
        kind[in->a] = 1;
        break;
      case LUAR_IR_CCALL1DI:
        kind[in->a] = 2;
        break;
      case LUAR_IR_CCALL1DD:
      case LUAR_IR_CCALL1FF:
        kind[in->a] = 2;
        break;
      case LUAR_IR_CCALL2DD:
      case LUAR_IR_CCALL2FF:
      case LUAR_IR_CCALL2DI:
      case LUAR_IR_CCALL2ID:
      case LUAR_IR_CCALL2FI:
      case LUAR_IR_CCALL2IF:
      case LUAR_IR_CCALL2FD:
      case LUAR_IR_CCALL2DF:
        kind[in->a] = 2;
        break;
      case LUAR_IR_CCALL2II:
        kind[in->a] = 1;
        break;
      case LUAR_IR_TGETK:
      case LUAR_IR_TGETI:
        kind[in->a] = 1;
        break;
      case LUAR_IR_TNEW:
        kind[in->a] = 3;
        break;
      case LUAR_IR_CCALL3DD:
      case LUAR_IR_CCALL3DDI:
      case LUAR_IR_CCALL3DID:
      case LUAR_IR_CCALL3IDD:
        kind[in->a] = 2;
        break;
      default:
        break;
    }
  }
  return 1;
}


int luar_trace_ir_link (LuaR_TraceIR *t, int parent_depth, int root_start) {
  int depth;
  if (t == NULL || root_start < 0)
    return 0;
  depth = parent_depth + 1;
  if (depth > LUAR_IR_MAX_DEPTH)
    return 0;
  t->depth = depth;
  /* A side trace that resumes exactly at the loop target is a cycle; the
  ** linker may reuse the root trace instead of compiling another level. */
  return (t->startpc == root_start) ? 2 : 1;
}


static int ir_tag (const TValue *v) {
  if (ttisinteger(v)) return LUAR_IR_TAG_INT;
  if (ttisfloat(v)) return LUAR_IR_TAG_FLOAT;
  if (ttistable(v)) return LUAR_IR_TAG_TABLE;
  if (ttisstring(v) && tsslen(tsvalue(v)) <= LUAI_MAXSHORTLEN)
    return LUAR_IR_TAG_STRING;
  if (ttisfulluserdata(v))
    return LUAR_IR_TAG_USERDATA;
  return LUAR_IR_TAG_OTHER;
}


static int inline_leaf_call (LuaR_TraceIR *t, int a, TValue *fn,
                              int pc) {
  LClosure *cl;
  Proto *fp;
  Instruction i0, i1;
  int rb, ra, k;
  if (!ttisLclosure(fn))
    return 0;
  cl = (LClosure *)fn->value_.gc;
  fp = cl->p;
  if (fp == NULL || fp->sizecode < 2 || isvararg(fp) ||
      cl->nupvalues != 0 || fp->maxstacksize < 2 || fp->numparams != 1)
    return 0;
  i0 = fp->code[0];
  k = 1;
  while (k < fp->sizecode) {
    int op = GET_OPCODE(fp->code[k]);
    if (op != OP_MMBIN && op != OP_MMBINI && op != OP_MMBINK)
      break;
    k++;
  }
  if (k >= fp->sizecode)
    return 0;
  i1 = fp->code[k];
  if (GET_OPCODE(i1) != OP_RETURN1)
    return 0;
  ra = GETARG_A(i1);
  rb = GETARG_B(i0);
  if (ra != 1 || rb != 0)
    return 0;
  switch (GET_OPCODE(i0)) {
    case OP_MULK:
    case OP_ADDK:
    case OP_SUBK: {
      TValue *kv;
      k = GETARG_C(i0);
      if (k < 0 || k >= fp->sizek)
        return 0;
      kv = &fp->k[k];
      if (!ttisinteger(kv))
        return 0;
      {
        LuaR_IRIns in;
        memset(&in, 0, sizeof(in));
        in.kind = GET_OPCODE(i0) == OP_MULK ? LUAR_IR_MULK :
                  GET_OPCODE(i0) == OP_ADDK ? LUAR_IR_ADDK : LUAR_IR_SUBK;
        in.a = (uint8_t)a;
        in.b = (uint8_t)(a + 1);
        in.ival = ivalue(kv);
        in.nval = (double)in.ival;
        in.pc = pc;
        return luar_trace_ir_emit(t, &in);
      }
    }
    case OP_ADDI: {
      LuaR_IRIns in;
      memset(&in, 0, sizeof(in));
      in.kind = LUAR_IR_ADDI;
      in.a = (uint8_t)a;
      in.b = (uint8_t)(a + 1);
      in.ival = GETARG_sC(i0);
      in.pc = pc;
      return luar_trace_ir_emit(t, &in);
    }
    default:
      return 0;
  }
}


static int inline_leaf_call2 (LuaR_TraceIR *t, int a, TValue *fn,
                               int pc) {
  LClosure *cl;
  Proto *fp;
  Instruction i0, i1;
  int ra, k;
  if (!ttisLclosure(fn))
    return 0;
  cl = (LClosure *)fn->value_.gc;
  fp = cl->p;
  if (fp == NULL || fp->sizecode < 2 || isvararg(fp) ||
      cl->nupvalues != 0 || fp->maxstacksize < 3 || fp->numparams != 2)
    return 0;
  i0 = fp->code[0];
  k = 1;
  while (k < fp->sizecode) {
    int op = GET_OPCODE(fp->code[k]);
    if (op != OP_MMBIN && op != OP_MMBINI && op != OP_MMBINK)
      break;
    k++;
  }
  if (k >= fp->sizecode)
    return 0;
  i1 = fp->code[k];
  if (GET_OPCODE(i1) != OP_RETURN1)
    return 0;
  ra = GETARG_A(i1);
  if (ra != GETARG_A(i0) || GETARG_B(i0) != 0 || GETARG_C(i0) != 1)
    return 0;
  switch (GET_OPCODE(i0)) {
    case OP_ADD:
    case OP_SUB:
    case OP_MUL: {
      LuaR_IRIns in;
      memset(&in, 0, sizeof(in));
      in.kind = GET_OPCODE(i0) == OP_ADD ? LUAR_IR_ADD :
                GET_OPCODE(i0) == OP_SUB ? LUAR_IR_SUB : LUAR_IR_MUL;
      in.a = (uint8_t)a;
      in.b = (uint8_t)(a + 1);
      in.c = (uint8_t)(a + 2);
      in.pc = pc;
      return luar_trace_ir_emit(t, &in);
    }
    default:
      return 0;
  }
}


/* Record-time proof that a CALL argument slot was just loaded with an
** integer value.  The live slot cannot be inspected: a cdata call goes
** through the __call metamethod, whose trampoline shifts the arguments up
** one slot before the call returns, so after the call base[a+1] holds the
** function cdata instead of the argument.  Instead, check that the
** preceding bytecode instruction (skipping MMBIN fallback markers, which
** the recorder ignores) writes this slot with an integer-producing
** operation. */
static int ir_call_arg_is_int (Proto *p, int pc, int argslot, TValue *base) {
  Instruction prev;
  int q;
  if (pc <= 0 || pc > p->sizecode)
    return 0;
  q = pc - 1;
  while (q > 0 &&
         (GET_OPCODE(p->code[q]) == OP_MMBIN ||
          GET_OPCODE(p->code[q]) == OP_MMBINI ||
          GET_OPCODE(p->code[q]) == OP_MMBINK))
    q--;
  prev = p->code[q];
  if (GETARG_A(prev) != argslot)
    return 0;
  switch (GET_OPCODE(prev)) {
    case OP_LOADI:
    case OP_ADDI:
      return 1;
    case OP_MOVE: {
      int src = GETARG_B(prev);
      return src >= 0 && src < p->maxstacksize && ttisinteger(&base[src]);
    }
    default:
      return 0;
  }
}


/* Record-time proof that a CALL argument slot was just loaded with a
** double value accepted by CCALL1DD: a MOVE from a live float slot or a
** LOADK of a float constant.  MMBIN fallback markers are skipped, like in
** the integer proof above.  Cdata reads and float arithmetic are proved
** from the recorded IR instead (see ir_last_op_writes_float). */
static int ir_find_arg_double (Proto *p, int q, int argslot, TValue *base) {
  Instruction prev;
  if (q < 0 || q >= p->sizecode)
    return -1;
  while (q > 0 &&
         (GET_OPCODE(p->code[q]) == OP_MMBIN ||
          GET_OPCODE(p->code[q]) == OP_MMBINI ||
          GET_OPCODE(p->code[q]) == OP_MMBINK))
    q--;
  prev = p->code[q];
  if (GETARG_A(prev) != argslot)
    return -1;
  switch (GET_OPCODE(prev)) {
    case OP_LOADF:
      return q;
    case OP_LOADK: {
      int k = GETARG_Bx(prev);
      if (k >= 0 && k < p->sizek && ttisfloat(&p->k[k]))
        return q;
      return -1;
    }
    case OP_MOVE: {
      int src = GETARG_B(prev);
      if (src >= 0 && src < p->maxstacksize && ttisfloat(&base[src]))
        return q;
      return -1;
    }
    default:
      return -1;
  }
}


static int ir_find_arg_int (Proto *p, int q, int argslot, TValue *base);

static int ir_call_args_ddi (Proto *p, int q, int argbase, TValue *base) {
  int q3 = ir_find_arg_int(p, q - 1, argbase + 2, base);
  int q2 = q3 >= 0 ? ir_find_arg_double(p, q3 - 1, argbase + 1, base) : -1;
  int q1 = q2 >= 0 ? ir_find_arg_double(p, q2 - 1, argbase, base) : -1;
  return q1 >= 0;
}


static int ir_call_args_if (Proto *p, int q, int argbase, TValue *base) {
  int qf = ir_find_arg_double(p, q - 1, argbase + 1, base);
  int qi = qf >= 0 ? ir_find_arg_int(p, qf - 1, argbase, base) : -1;
  return qf >= 0 && qi >= 0;
}


static int ir_call_args_fi (Proto *p, int q, int argbase, TValue *base) {
  int qi = ir_find_arg_int(p, q - 1, argbase + 1, base);
  int qf = qi >= 0 ? ir_find_arg_double(p, qi - 1, argbase, base) : -1;
  return qi >= 0 && qf >= 0;
}


static int ir_call_args_id (Proto *p, int q, int argbase, TValue *base) {
  int qd = ir_find_arg_double(p, q - 1, argbase + 1, base);
  int qi = qd >= 0 ? ir_find_arg_int(p, qd - 1, argbase, base) : -1;
  return qd >= 0 && qi >= 0;
}


static int ir_find_arg_int (Proto *p, int q, int argslot, TValue *base) {
  Instruction prev;
  if (q < 0 || q >= p->sizecode)
    return -1;
  while (q > 0 &&
         (GET_OPCODE(p->code[q]) == OP_MMBIN ||
          GET_OPCODE(p->code[q]) == OP_MMBINI ||
          GET_OPCODE(p->code[q]) == OP_MMBINK))
    q--;
  prev = p->code[q];
  if (GETARG_A(prev) != argslot)
    return -1;
  switch (GET_OPCODE(prev)) {
    case OP_LOADI:
    case OP_ADDI:
      return q;
    case OP_MOVE: {
      int src = GETARG_B(prev);
      if (src >= 0 && src < p->maxstacksize && ttisinteger(&base[src]))
        return q;
      return -1;
    }
    default:
      return -1;
  }
}


static int ir_call_arg_is_double (Proto *p, int pc, int argslot,
                                  TValue *base) {
  if (pc <= 0 || pc > p->sizecode)
    return 0;
  return ir_find_arg_double(p, pc - 1, argslot, base) >= 0;
}


/* Two-argument form: arg2 is the last argument-producing instruction
** before CALL, arg1 immediately precedes it (MMBIN markers skipped). */
static int ir_call_args2_are_double (Proto *p, int pc, int arg1, int arg2,
                                     TValue *base) {
  int q2, q1;
  if (pc <= 0 || pc > p->sizecode)
    return 0;
  q2 = ir_find_arg_double(p, pc - 1, arg2, base);
  if (q2 < 0)
    return 0;
  q1 = ir_find_arg_double(p, q2 - 1, arg1, base);
  return q1 >= 0;
}


/* True when instruction `kind` writes a stack slot in `a`.  Mutating
** instructions (stores, table writes, guards and control flow) keep their
** operand in `a`, so a backward scan for the producer of a value slot must
** step over them instead of treating them as writes. */
static int ir_writes_slot (uint8_t kind) {
  switch (kind) {
    case LUAR_IR_SLOAD:
    case LUAR_IR_ULOAD:
    case LUAR_IR_SSTORE:
    case LUAR_IR_LOADI:
    case LUAR_IR_LOADF:
    case LUAR_IR_ADDI:
    case LUAR_IR_ADDK:
    case LUAR_IR_SUBK:
    case LUAR_IR_MULK:
    case LUAR_IR_DIVK:
    case LUAR_IR_MODK:
    case LUAR_IR_IDIVK:
    case LUAR_IR_ADD:
    case LUAR_IR_SUB:
    case LUAR_IR_MUL:
    case LUAR_IR_DIV:
    case LUAR_IR_TGET:
    case LUAR_IR_CGETI:
    case LUAR_IR_CGETF:
    case LUAR_IR_CGETFIELD:
    case LUAR_IR_CPGETI:
    case LUAR_IR_CPGETI64:
    case LUAR_IR_CPGETF:
    case LUAR_IR_CCALL0I:
    case LUAR_IR_CCALL1I:
    case LUAR_IR_CCALL1DI:
    case LUAR_IR_CCALL1DD:
    case LUAR_IR_CCALL1FF:
    case LUAR_IR_CCALL2II:
    case LUAR_IR_CCALL2DD:
    case LUAR_IR_CCALL3DD:
    case LUAR_IR_CCALL3DDI:
    case LUAR_IR_CCALL3DID:
    case LUAR_IR_CCALL3IDD:
    case LUAR_IR_TGETF:
    case LUAR_IR_ITOF:
    case LUAR_IR_FADDI:
    case LUAR_IR_FMULK:
    case LUAR_IR_FADD:
    case LUAR_IR_FSUB:
    case LUAR_IR_FDIVK:
      return 1;
    default:
      return 0;
  }
}


/* True when the latest IR writer of `slot` before the current end is an op
** of `kind`.  Used to ensure a recorded ULOAD value is still the current
** function-slot value at CALL time. */
static int ir_latest_writer_is (const LuaR_TraceIR *t, int slot,
                                uint8_t kind) {
  int i;
  for (i = t->nins - 1; i >= 0; i--) {
    if (t->ins[i].a != slot || !ir_writes_slot(t->ins[i].kind))
      continue;
    return t->ins[i].kind == kind;
  }
  return 0;
}


static int ir_slot_const_int (const LuaR_TraceIR *t, int slot,
                             TValue *known, int64_t *out) {
  int i;
  for (i = t->nins - 1; i >= 0; i--) {
    if (t->ins[i].a != slot || !ir_writes_slot(t->ins[i].kind))
      continue;
    if (t->ins[i].kind == LUAR_IR_LOADI) {
      *out = t->ins[i].ival;
      return 1;
    }
    if (t->ins[i].kind == LUAR_IR_ULOAD && known != NULL &&
        known[slot].tt_ != 0 && ttisinteger(&known[slot])) {
      *out = ivalue(&known[slot]);
      return 1;
    }
    return 0;
  }
  return 0;
}




static int ir_slot_is_int (const LuaR_TraceIR *t, int slot, int end,
                           int depth) {
  int i;
  if (depth > 8)
    return 0;
  for (i = end - 1; i >= 0; i--) {
    if (t->ins[i].kind == LUAR_IR_GUARD && t->ins[i].a == slot)
      return t->ins[i].b == LUAR_IR_TAG_INT;
    if (t->ins[i].a != slot || !ir_writes_slot(t->ins[i].kind))
      continue;
    if (t->ins[i].kind == LUAR_IR_LOADI)
      return 1;
    if (t->ins[i].kind == LUAR_IR_SLOAD)
      return ir_slot_is_int(t, t->ins[i].b, i, depth + 1);
    return 0;
  }
  return 0;
}


static int ir_ins_is_float (const LuaR_TraceIR *t, int i, int depth);


/* Does the latest writer of `slot` before instruction `end` carry a float
** tag at runtime?  Only ops whose result is provably a Lua float qualify:
** `/` is always float, `*` is float with a float constant or a float
** operand, and `+`/`-` are float when either operand is float.  Unknown
** producers return false so the call stays on the interpreter path. */
static int ir_slot_is_float (const LuaR_TraceIR *t, int slot, int end,
                             int depth) {
  int i;
  if (depth > 8)
    return 0;
  for (i = end - 1; i >= 0; i--) {
    if (t->ins[i].a != slot || !ir_writes_slot(t->ins[i].kind))
      continue;
    return ir_ins_is_float(t, i, depth);
  }
  return 0;
}


static int ir_ins_is_float (const LuaR_TraceIR *t, int i, int depth) {
  const LuaR_IRIns *in = &t->ins[i];
  switch (in->kind) {
    case LUAR_IR_LOADF:
    case LUAR_IR_CGETF:
    case LUAR_IR_CPGETF:
    case LUAR_IR_CCALL1DI:
    case LUAR_IR_CCALL1DD:
    case LUAR_IR_CCALL1FF:
    case LUAR_IR_CCALL2DD:
    case LUAR_IR_CCALL3DD:
    case LUAR_IR_TGETF:
    case LUAR_IR_ITOF:
    case LUAR_IR_FADDI:
    case LUAR_IR_FMULK:
    case LUAR_IR_FADD:
    case LUAR_IR_FSUB:
    case LUAR_IR_FDIVK:
      return 1;
    case LUAR_IR_CGETFIELD:
      return in->c == 2;
    case LUAR_IR_DIVK:
      return 1;   /* Lua `/` always produces a float */
    case LUAR_IR_MULK:
      return in->nval != (double)in->ival ||
             ir_slot_is_float(t, in->b, i, depth + 1);
    case LUAR_IR_ADD:
    case LUAR_IR_SUB:
      return ir_slot_is_float(t, in->b, i, depth + 1) ||
             ir_slot_is_float(t, in->c, i, depth + 1);
    case LUAR_IR_SLOAD:
      return ir_slot_is_float(t, in->b, i, depth + 1);
    default:
      return 0;
  }
}


/* The last recorded IR op must produce the float that the CALL argument
** slot holds.  This covers direct cdata reads (CGETF/CPGETF/
** CGETFIELD-float) and float arithmetic on them (MULK, DIVK, ADD, SUB);
** ir_floatify later turns the arithmetic into the corresponding float op.
** The guard emitted before CCALL1DD still validates the runtime tag. */
static int ir_last_op_writes_float (const LuaR_TraceIR *t, int slot) {
  int last;
  if (t == NULL || t->nins <= 0)
    return 0;
  last = t->nins - 1;
  if (t->ins[last].a != slot || !ir_writes_slot(t->ins[last].kind))
    return 0;
  return ir_ins_is_float(t, last, 0);
}


/* Three-argument form: arg3 is the last producer before CALL, then arg2,
** then arg1, each MOVE from a live float slot or LOADK float. */
static int ir_call_args3_are_double (Proto *p, int pc, int arg1, int arg2,
                                     int arg3, TValue *base) {
  int q3, q2, q1;
  if (pc <= 0 || pc > p->sizecode)
    return 0;
  q3 = ir_find_arg_double(p, pc - 1, arg3, base);
  if (q3 < 0)
    return 0;
  q2 = ir_find_arg_double(p, q3 - 1, arg2, base);
  if (q2 < 0)
    return 0;
  q1 = ir_find_arg_double(p, q2 - 1, arg1, base);
  return q1 >= 0;
}


static LClosure *ir_current_closure (lua_State *L) {
  CallInfo *ci;
  if (L == NULL || (ci = L->ci) == NULL)
    return NULL;
  if (!ttisLclosure(s2v(ci->func.p)))
    return NULL;
  return clLvalue(s2v(ci->func.p));
}


static int ir_is_cdata_function (TValue *v) {
  return luar_ffi_trace_ccall0_kind(v) > 0 ||
         luar_ffi_trace_ccall1_kind(v) > 0 ||
         luar_ffi_trace_ccall1di_kind(v) > 0 ||
         luar_ffi_trace_ccall1dd_kind(v) > 0 ||
         luar_ffi_trace_ccall1ff_kind(v) > 0 ||
         luar_ffi_trace_ccall2dd_kind(v) > 0 ||
         luar_ffi_trace_ccall2ff_kind(v) > 0 ||
         luar_ffi_trace_ccall2di_kind(v) > 0 ||
         luar_ffi_trace_ccall2id_kind(v) > 0 ||
         luar_ffi_trace_ccall2fi_kind(v) > 0 ||
         luar_ffi_trace_ccall2if_kind(v) > 0 ||
         luar_ffi_trace_ccall2fd_kind(v) > 0 ||
         luar_ffi_trace_ccall2df_kind(v) > 0 ||
         luar_ffi_trace_ccall3ddi_kind(v) > 0 ||
         luar_ffi_trace_ccall3did_kind(v) > 0 ||
         luar_ffi_trace_ccall3idd_kind(v) > 0 ||
         luar_ffi_trace_ccall2ii_kind(v) > 0 ||
         luar_ffi_trace_ccall3dd_kind(v) > 0;
}


static int record_loop (Proto *p, int start, int backedge, TValue *base,
                        lua_State *L, LuaR_TraceIR *t, int partial,
                        int *callpc, int suffix) {
  int pc, ns = p->maxstacksize;
  int seen[LUAR_IR_MAX_SLOTS];
  int fn_src[LUAR_IR_MAX_SLOTS];
  int known[LUAR_IR_MAX_SLOTS];
  TValue known_fn[LUAR_IR_MAX_SLOTS];
  TValue known_cval[LUAR_IR_MAX_SLOTS];
  LuaR_IRIns in;
  LuaR_IRExitState ex;
  int sawloop = 0, partial_done = 0;
  if (callpc != NULL) *callpc = -1;
  if (ns > LUAR_IR_MAX_SLOTS) ns = LUAR_IR_MAX_SLOTS;
  luar_trace_ir_init(t, p, start, ns, 0);
  memset(seen, 0, sizeof(seen));
  memset(fn_src, -1, sizeof(fn_src));
  memset(known, 0, sizeof(known));
  memset(known_fn, 0, sizeof(known_fn));
  memset(known_cval, 0, sizeof(known_cval));
  memset(&in, 0, sizeof(in));
  for (pc = start; pc <= backedge; pc++) {
    Instruction ins = p->code[pc];
    int a = GETARG_A(ins), b = GETARG_B(ins), c = GETARG_C(ins);
    int r;
    if (suffix && pc == backedge) {
      in.kind = LUAR_IR_RET; in.pc = pc;
      return luar_trace_ir_emit(t, &in);
    }
    if (a >= ns || (b >= ns && GET_OPCODE(ins) != OP_MMBINK &&
                    GET_OPCODE(ins) != OP_MMBINI &&
                    GET_OPCODE(ins) != OP_LOADK &&
                    GET_OPCODE(ins) != OP_LOADI &&
                    GET_OPCODE(ins) != OP_LOADF &&
                    GET_OPCODE(ins) != OP_FORLOOP &&
                    GET_OPCODE(ins) != OP_NEWTABLE &&
                    GET_OPCODE(ins) != OP_SETLIST) ||
        (c >= ns && (GET_OPCODE(ins) == OP_ADD ||
                     GET_OPCODE(ins) == OP_SUB ||
                     GET_OPCODE(ins) == OP_MUL ||
                     GET_OPCODE(ins) == OP_DIV))) {
      return 0;
    }
    switch (GET_OPCODE(ins)) {
      case OP_GETUPVAL: {
        LClosure *cl = ir_current_closure(L);
        TValue uv;
        int idx = b;
        if (cl == NULL || idx < 0 || idx >= (int)cl->nupvalues ||
            cl->upvals[idx] == NULL || cl->upvals[idx]->v.p == NULL)
          return 0;
        uv = *cl->upvals[idx]->v.p;
        in.kind = LUAR_IR_ULOAD; in.a = (uint8_t)a;
        in.ival = (int64_t)(intptr_t)cl;
        in.nval = (double)idx;
        in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        if (!r) return 0;
        if (ir_is_cdata_function(&uv)) {
          known_fn[a] = uv;
          known[a] = 1;
        }
        else if (ttisboolean(&uv)) {
          /* Dynamic-key writes validate the value tag at runtime; no
          ** boolean guard tag is needed for this ULOAD. */
          known_cval[a] = uv;
          seen[a] = 1;
        }
        else {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
          in.b = (uint8_t)ir_tag(&uv); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          if (r)
            known_cval[a] = uv;
        }
        break;
      }
      case OP_MOVE:
        if (ttisLclosure(&base[b])) {
          fn_src[a] = b;
          in.kind = LUAR_IR_SLOAD; in.a = (uint8_t)a; in.b = (uint8_t)b;
          in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (partial && b < ns && ir_tag(&base[b]) == LUAR_IR_TAG_OTHER) {
          in.kind = LUAR_IR_SLOAD; in.a = (uint8_t)a; in.b = (uint8_t)b;
          in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (luar_ffi_trace_ccall0_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall1_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall1di_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall1dd_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall1ff_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall2dd_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall2ff_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall2di_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall2ii_kind(&base[b]) > 0 ||
            luar_ffi_trace_ccall3dd_kind(&base[b]) > 0) {
          in.kind = LUAR_IR_SLOAD; in.a = (uint8_t)a; in.b = (uint8_t)b;
          in.pc = pc;
          fn_src[a] = b;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!seen[b] && b < ns && !ttisstring(&base[b])) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)b;
          in.b = (uint8_t)ir_tag(&base[b]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[b] = 1;
        }
        in.kind = LUAR_IR_SLOAD; in.a = (uint8_t)a; in.b = (uint8_t)b;
        in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      case OP_LOADI:
        in.kind = LUAR_IR_LOADI; in.a = (uint8_t)a;
        in.ival = GETARG_sBx(ins); in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      case OP_LOADF:
        in.kind = LUAR_IR_LOADF; in.a = (uint8_t)a;
        in.nval = (double)GETARG_sBx(ins); in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      case OP_LOADTRUE:
      case OP_LOADFALSE: {
        in.kind = LUAR_IR_KBOOL; in.a = (uint8_t)a;
        in.nval = GET_OPCODE(ins) == OP_LOADTRUE ? 1.0 : 0.0;
        in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_LOADK: {
        int k = GETARG_Bx(ins);
        if (k < 0 || k >= p->sizek)
          return 0;
        if (ttisfloat(&p->k[k])) {
          in.kind = LUAR_IR_LOADF; in.a = (uint8_t)a;
          in.nval = fltvalue(&p->k[k]); in.pc = pc;
        }
        else if (ttisstring(&p->k[k])) {
          in.kind = LUAR_IR_KSTR; in.a = (uint8_t)a;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[k]); in.pc = pc;
        }
        else if (ttisboolean(&p->k[k])) {
          in.kind = LUAR_IR_KBOOL; in.a = (uint8_t)a;
          in.nval = ttistrue(&p->k[k]) ? 1.0 : 0.0; in.pc = pc;
        }
        else
          return 0;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_ADDI: {
        TValue *sv = (ir_latest_writer_is(t, b, LUAR_IR_ULOAD) &&
                      known_cval[b].tt_ != 0) ?
                     &known_cval[b] : &base[b];
        if (ttisfloat(sv)) {
          in.kind = LUAR_IR_FADDI; in.a = (uint8_t)a;
          in.b = (uint8_t)b; in.nval = (double)GETARG_sC(ins);
          in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        in.kind = LUAR_IR_ADDI; in.a = (uint8_t)a; in.b = (uint8_t)b;
        in.ival = GETARG_sC(ins); in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_ADD:
      case OP_SUB:
      case OP_MUL:
      case OP_DIV:
        if (!seen[b]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)b;
          in.b = (uint8_t)ir_tag(&base[b]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[b] = 1;
        }
        if (!seen[c]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)c;
          in.b = (uint8_t)ir_tag(&base[c]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[c] = 1;
        }
        in.kind = (GET_OPCODE(ins) == OP_ADD) ? LUAR_IR_ADD :
                  (GET_OPCODE(ins) == OP_SUB) ? LUAR_IR_SUB :
                  (GET_OPCODE(ins) == OP_MUL) ? LUAR_IR_MUL : LUAR_IR_DIV;
        in.a = (uint8_t)a; in.b = (uint8_t)b; in.c = (uint8_t)c;
        in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      case OP_ADDK:
      case OP_SUBK:
      case OP_MULK:
      case OP_DIVK:
      case OP_MODK:
      case OP_IDIVK:
        if (!seen[b]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)b;
          in.b = (uint8_t)ir_tag(&base[b]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[b] = 1;
        }
        in.kind = (GET_OPCODE(ins) == OP_ADDK) ? LUAR_IR_ADDK :
                  (GET_OPCODE(ins) == OP_SUBK) ? LUAR_IR_SUBK :
                  (GET_OPCODE(ins) == OP_MULK) ? LUAR_IR_MULK :
                  (GET_OPCODE(ins) == OP_DIVK) ? LUAR_IR_DIVK :
                  (GET_OPCODE(ins) == OP_MODK) ? LUAR_IR_MODK :
                                                  LUAR_IR_IDIVK;
        in.a = (uint8_t)a; in.b = (uint8_t)b; in.c = (uint8_t)c;
        in.ival = ttisinteger(&p->k[c]) ? ivalue(&p->k[c]) : 0;
        in.nval = ttisfloat(&p->k[c]) ? fltvalue(&p->k[c]) :
                  (double)in.ival;
        in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      case OP_GETTABUP: {
        LClosure *cl = ir_current_closure(L);
        TValue uv, curv;
        TString *sname;
        if (cl == NULL || b < 0 || b >= (int)cl->nupvalues ||
            cl->upvals[b] == NULL || cl->upvals[b]->v.p == NULL ||
            c >= p->sizek || !ttisstring(&p->k[c]))
          return 0;
        uv = *cl->upvals[b]->v.p;
        sname = tsvalue(&p->k[c]);
        if (!ttistable(&uv))
          return 0;
        luaH_getstr((Table *)uv.value_.gc, sname, &curv);
        if (!ttisinteger(&curv) && !ttisfloat(&curv) &&
            !ttisstring(&curv) && !ttisboolean(&curv) &&
            !ttistable(&curv))
          return 0;
        in.kind = LUAR_IR_ULOAD; in.a = (uint8_t)a;
        in.ival = (int64_t)(intptr_t)cl; in.nval = (double)b;
        in.pc = pc;
        if (!luar_trace_ir_emit(t, &in)) return 0;
        in.kind = ttisstring(&curv) ? LUAR_IR_TGETKS :
                  ttisfloat(&curv) ? LUAR_IR_TGETKF :
                  ttisboolean(&curv) ? LUAR_IR_TGETKB :
                  ttistable(&curv) ? LUAR_IR_TGETKT : LUAR_IR_TGETK;
        in.a = (uint8_t)a; in.b = (uint8_t)a;
        in.ival = (int64_t)(intptr_t)sname; in.pc = pc;
        seen[a] = 1;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_GETFIELD: {
        int fk;
        size_t off = 0, fsize = 0;
        if (!ttisstring(&p->k[c])) {
          return 0;
        }
        {
          TValue *tv = (ir_latest_writer_is(t, b, LUAR_IR_ULOAD) &&
                        known_cval[b].tt_ != 0) ?
                       &known_cval[b] : &base[b];
        if (ttistable(tv)) {
          TString *sname = tsvalue(&p->k[c]);
          TValue curv;
          luaH_getstr((Table *)tv->value_.gc, sname, &curv);
          if (!ttisinteger(&curv) && !ttisfloat(&curv) &&
              !ttisstring(&curv) && !ttisboolean(&curv) &&
              !ttistable(&curv))
            return 0;
          if (!seen[b]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)b;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[b] = 1;
          }
          in.kind = ttisstring(&curv) ? LUAR_IR_TGETKS :
                    ttisfloat(&curv) ? LUAR_IR_TGETKF :
                    ttisboolean(&curv) ? LUAR_IR_TGETKB :
                    ttistable(&curv) ? LUAR_IR_TGETKT : LUAR_IR_TGETK;
          in.a = (uint8_t)a;
          in.b = (uint8_t)b;
          in.ival = (int64_t)(intptr_t)sname;
          in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        }
        if (pc + 5 <= backedge &&
            luar_ffi_trace_kind(&base[b]) == 3) {
          Instruction r1 = p->code[pc + 1], r2 = p->code[pc + 2];
          Instruction r3 = p->code[pc + 3], r4 = p->code[pc + 4];
          Instruction r5 = p->code[pc + 5];
          size_t poff = 0, pesz = 0;
          int pk = 0;
          if (GET_OPCODE(r1) == OP_ADD && GET_OPCODE(r2) == OP_MMBIN &&
              GET_OPCODE(r3) == OP_ADD && GET_OPCODE(r4) == OP_MMBIN &&
              GET_OPCODE(r5) == OP_GETI &&
              GETARG_A(r1) == a && GETARG_B(r1) == a &&
              GETARG_A(r3) == a && GETARG_B(r3) == a &&
              GETARG_B(r5) == a && GETARG_C(r5) == 0 &&
              GETARG_C(r1) < ns && GETARG_C(r3) < ns &&
              ttisinteger(&base[GETARG_C(r1)]) &&
              ttisinteger(&base[GETARG_C(r3)]) &&
              luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                       &pk, &poff, &pesz)) {
            int da = GETARG_A(r5), k1 = GETARG_C(r1), k2 = GETARG_C(r3);
            if (da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (!seen[k1]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)k1;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[k1] = 1;
            }
            if (!seen[k2]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)k2;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[k2] = 1;
            }
            in.kind = LUAR_IR_ADD; in.a = (uint8_t)a;
            in.b = (uint8_t)k1; in.c = (uint8_t)k2; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
            in.kind = pk == 2 ? LUAR_IR_CPGETF :
                      (pesz == 8 ? LUAR_IR_CPGETI64 : LUAR_IR_CPGETI);
            in.a = (uint8_t)da; in.b = (uint8_t)b; in.c = (uint8_t)a;
            in.ival = (int64_t)poff; in.nval = (double)pesz;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 5;
            break;
          }
        }
        if (pc + 3 <= backedge &&
            luar_ffi_trace_kind(&base[b]) == 3) {
          Instruction q1 = p->code[pc + 1], q2 = p->code[pc + 2];
          if (GET_OPCODE(q1) == OP_ADD && GET_OPCODE(q2) == OP_MMBIN &&
              GETARG_A(q1) == a && GETARG_B(q1) == a &&
              GETARG_C(q1) < ns && ttisinteger(&base[GETARG_C(q1)])) {
            int pp = pc + 3, steps = 0, offsum = 0;
            size_t poff = 0, pesz = 0;
            int pk = 0;
            while (pp + 1 <= backedge) {
              Instruction di = p->code[pp], mi = p->code[pp + 1];
              if (GET_OPCODE(di) == OP_ADDI && GET_OPCODE(mi) == OP_MMBINI &&
                  GETARG_A(di) == a && GETARG_B(di) == a) {
                offsum += GETARG_sC(di);
                pp += 2;
                steps++;
                continue;
              }
              break;
            }
            if (steps > 0 && pp <= backedge &&
                GET_OPCODE(p->code[pp]) == OP_SETI &&
                GETARG_A(p->code[pp]) == a &&
                GETARG_B(p->code[pp]) == 0 &&
                !GETARG_k(p->code[pp]) &&
                luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                         &pk, &poff, &pesz)) {
              int vc = GETARG_C(p->code[pp]), ks = GETARG_C(q1);
              TValue *v = &base[vc];
              if (pk == 1 ? !ttisinteger(v) : !ttisfloat(v)) return 0;
              if (!seen[b]) {
                in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
                in.ival = (int64_t)(intptr_t)base[b].value_.gc;
                in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[b] = 1;
              }
              if (!seen[ks]) {
                in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
                in.b = LUAR_IR_TAG_INT; in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[ks] = 1;
              }
              if (!seen[vc]) {
                in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vc;
                in.b = (uint8_t)(pk == 2 ? LUAR_IR_TAG_FLOAT :
                                            LUAR_IR_TAG_INT);
                in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[vc] = 1;
              }
              in.kind = LUAR_IR_ADDI; in.a = (uint8_t)a;
              in.b = (uint8_t)ks; in.ival = offsum; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[a] = 1;
              in.kind = pk == 2 ? LUAR_IR_CPSETF :
                        (pesz == 8 ? LUAR_IR_CPSETI64 : LUAR_IR_CPSETI);
              in.a = (uint8_t)b; in.b = (uint8_t)a; in.c = (uint8_t)vc;
              in.ival = (int64_t)poff; in.nval = (double)pesz;
              in.pc = pc;
              r = luar_trace_ir_emit(t, &in);
              if (r) pc = pp;
              break;
            }
          }
        }
        if (pc + 3 <= backedge &&
            luar_ffi_trace_kind(&base[b]) == 3) {
          Instruction q1 = p->code[pc + 1], q2 = p->code[pc + 2];
          if (GET_OPCODE(q1) == OP_ADD && GET_OPCODE(q2) == OP_MMBIN &&
              GETARG_A(q1) == a && GETARG_B(q1) == a &&
              GETARG_C(q1) < ns && ttisinteger(&base[GETARG_C(q1)])) {
            int pp = pc + 3, steps = 0, offsum = 0;
            size_t poff = 0, pesz = 0;
            int pk = 0;
            while (pp + 1 <= backedge) {
              Instruction di = p->code[pp], mi = p->code[pp + 1];
              if (GET_OPCODE(di) == OP_ADDI && GET_OPCODE(mi) == OP_MMBINI &&
                  GETARG_A(di) == a && GETARG_B(di) == a) {
                offsum += GETARG_sC(di);
                pp += 2;
                steps++;
                continue;
              }
              break;
            }
            if (steps > 0 && pp <= backedge &&
                GET_OPCODE(p->code[pp]) == OP_GETI &&
                GETARG_B(p->code[pp]) == a &&
                GETARG_C(p->code[pp]) == 0 &&
                luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                         &pk, &poff, &pesz)) {
              int da = GETARG_A(p->code[pp]), ks = GETARG_C(q1);
              if (da >= ns) return 0;
              if (!seen[b]) {
                in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
                in.ival = (int64_t)(intptr_t)base[b].value_.gc;
                in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[b] = 1;
              }
              if (!seen[ks]) {
                in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
                in.b = LUAR_IR_TAG_INT; in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[ks] = 1;
              }
              in.kind = LUAR_IR_ADDI; in.a = (uint8_t)a;
              in.b = (uint8_t)ks; in.ival = offsum; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[a] = 1;
              in.kind = pk == 2 ? LUAR_IR_CPGETF :
                        (pesz == 8 ? LUAR_IR_CPGETI64 : LUAR_IR_CPGETI);
              in.a = (uint8_t)da; in.b = (uint8_t)b; in.c = (uint8_t)a;
              in.ival = (int64_t)poff; in.nval = (double)pesz;
              in.pc = pc;
              seen[da] = 1;
              r = luar_trace_ir_emit(t, &in);
              if (r) pc = pp;
              break;
            }
          }
        }
        if (pc + 7 <= backedge) {
          Instruction a1 = p->code[pc + 1], m1 = p->code[pc + 2];
          Instruction a2 = p->code[pc + 3], m2 = p->code[pc + 4];
          Instruction a3 = p->code[pc + 5], m3 = p->code[pc + 6];
          Instruction g2 = p->code[pc + 7];
          size_t poff = 0, pesz = 0;
          int pk = 0;
          if (GET_OPCODE(a1) == OP_ADD && GET_OPCODE(m1) == OP_MMBIN &&
              GET_OPCODE(a2) == OP_ADDI && GET_OPCODE(a3) == OP_ADDI &&
              GET_OPCODE(m2) == OP_MMBINI && GET_OPCODE(m3) == OP_MMBINI &&
              GET_OPCODE(g2) == OP_GETI &&
              GETARG_A(a1) == a && GETARG_B(a1) == a &&
              GETARG_A(a2) == a && GETARG_B(a2) == a &&
              GETARG_A(a3) == a && GETARG_B(a3) == a &&
              GETARG_B(g2) == a && GETARG_C(g2) == 0 &&
              GETARG_C(a1) < ns && ttisinteger(&base[GETARG_C(a1)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                       &pk, &poff, &pesz)) {
            int da = GETARG_A(g2), ks = GETARG_C(a1);
            int offsum = GETARG_sC(a2) + GETARG_sC(a3);
            if (da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (!seen[ks]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[ks] = 1;
            }
            in.kind = LUAR_IR_ADDI; in.a = (uint8_t)a;
            in.b = (uint8_t)ks; in.ival = offsum; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
            in.kind = pk == 2 ? LUAR_IR_CPGETF :
                      (pesz == 8 ? LUAR_IR_CPGETI64 : LUAR_IR_CPGETI);
            in.a = (uint8_t)da; in.b = (uint8_t)b; in.c = (uint8_t)a;
            in.ival = (int64_t)poff; in.nval = (double)pesz;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 7;
            break;
          }
        }
        if (pc + 3 <= backedge) {
          Instruction ai = p->code[pc + 1], mi = p->code[pc + 2];
          Instruction gi = p->code[pc + 3];
          size_t poff = 0, pesz = 0;
          int pk = 0;
          if (pc + 5 <= backedge) {
            Instruction q1 = p->code[pc + 1], q2 = p->code[pc + 2];
            Instruction q3 = p->code[pc + 3], q4 = p->code[pc + 4];
            Instruction q5 = p->code[pc + 5];
            if (GET_OPCODE(q1) == OP_ADD && GET_OPCODE(q2) == OP_MMBIN &&
                GET_OPCODE(q3) == OP_ADDI && GET_OPCODE(q4) == OP_MMBINI &&
                GET_OPCODE(q5) == OP_SETI &&
                GETARG_A(q1) == a && GETARG_B(q1) == a &&
                GETARG_A(q3) == a && GETARG_B(q3) == a &&
                GETARG_A(q5) == a && GETARG_B(q5) == 0 &&
                !GETARG_k(q5) &&
                GETARG_C(q1) < ns && ttisinteger(&base[GETARG_C(q1)]) &&
                luar_ffi_trace_kind(&base[b]) == 3 &&
                luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                         &pk, &poff, &pesz)) {
              int vc = GETARG_C(q5), ks = GETARG_C(q1);
              TValue *v = &base[vc];
              if (pk == 1 ? !ttisinteger(v) : !ttisfloat(v)) return 0;
              if (!seen[b]) {
                in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
                in.ival = (int64_t)(intptr_t)base[b].value_.gc;
                in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[b] = 1;
              }
              if (!seen[ks]) {
                in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
                in.b = LUAR_IR_TAG_INT; in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[ks] = 1;
              }
              if (!seen[vc]) {
                in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vc;
                in.b = (uint8_t)(pk == 2 ? LUAR_IR_TAG_FLOAT :
                                            LUAR_IR_TAG_INT);
                in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[vc] = 1;
              }
              in.kind = LUAR_IR_ADDI; in.a = (uint8_t)a;
              in.b = (uint8_t)ks; in.ival = GETARG_sC(q3); in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[a] = 1;
              in.kind = pk == 2 ? LUAR_IR_CPSETF :
                        (pesz == 8 ? LUAR_IR_CPSETI64 : LUAR_IR_CPSETI);
              in.a = (uint8_t)b; in.b = (uint8_t)a; in.c = (uint8_t)vc;
              in.ival = (int64_t)poff; in.nval = (double)pesz;
              in.pc = pc;
              r = luar_trace_ir_emit(t, &in);
              if (r) pc += 5;
              break;
            }
            if (GET_OPCODE(q1) == OP_ADD && GET_OPCODE(q2) == OP_MMBIN &&
                GET_OPCODE(q3) == OP_ADDI && GET_OPCODE(q4) == OP_MMBINI &&
                GET_OPCODE(q5) == OP_GETI &&
                GETARG_A(q1) == a && GETARG_B(q1) == a &&
                GETARG_A(q3) == a && GETARG_B(q3) == a &&
                GETARG_B(q5) == a && GETARG_C(q5) == 0 &&
                GETARG_C(q1) < ns && ttisinteger(&base[GETARG_C(q1)]) &&
                luar_ffi_trace_kind(&base[b]) == 3 &&
                luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                         &pk, &poff, &pesz)) {
              int da = GETARG_A(q5), ks = GETARG_C(q1);
              if (da >= ns) return 0;
              if (!seen[b]) {
                in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
                in.ival = (int64_t)(intptr_t)base[b].value_.gc;
                in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[b] = 1;
              }
              if (!seen[ks]) {
                in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
                in.b = LUAR_IR_TAG_INT; in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[ks] = 1;
              }
              in.kind = LUAR_IR_ADDI; in.a = (uint8_t)a;
              in.b = (uint8_t)ks; in.ival = GETARG_sC(q3); in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[a] = 1;
              in.kind = pk == 2 ? LUAR_IR_CPGETF :
                        (pesz == 8 ? LUAR_IR_CPGETI64 : LUAR_IR_CPGETI);
              in.a = (uint8_t)da; in.b = (uint8_t)b; in.c = (uint8_t)a;
              in.ival = (int64_t)poff; in.nval = (double)pesz;
              in.pc = pc;
              seen[da] = 1;
              r = luar_trace_ir_emit(t, &in);
              if (r) pc += 5;
              break;
            }
          }
          if (GET_OPCODE(ai) == OP_ADD && GET_OPCODE(mi) == OP_MMBIN &&
              GET_OPCODE(gi) == OP_SETI &&
              GETARG_A(ai) == a && GETARG_B(ai) == a &&
              GETARG_A(gi) == a && GETARG_B(gi) == 0 &&
              !GETARG_k(gi) &&
              GETARG_C(ai) < ns && ttisinteger(&base[GETARG_C(ai)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                       &pk, &poff, &pesz)) {
            int vc = GETARG_C(gi), ks = GETARG_C(ai);
            TValue *v = &base[vc];
            if (pk == 1 ? !ttisinteger(v) : !ttisfloat(v)) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (!seen[ks]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[ks] = 1;
            }
            if (!seen[vc]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vc;
              in.b = (uint8_t)(pk == 2 ? LUAR_IR_TAG_FLOAT : LUAR_IR_TAG_INT);
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[vc] = 1;
            }
            in.kind = pk == 2 ? LUAR_IR_CPSETF :
                      (pesz == 8 ? LUAR_IR_CPSETI64 : LUAR_IR_CPSETI);
            in.a = (uint8_t)b; in.b = (uint8_t)ks; in.c = (uint8_t)vc;
            in.ival = (int64_t)poff; in.nval = (double)pesz;
            in.pc = pc;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 3;
            break;
          }
          if (GET_OPCODE(ai) == OP_ADD && GET_OPCODE(mi) == OP_MMBIN &&
              GET_OPCODE(gi) == OP_GETI &&
              GETARG_A(ai) == a && GETARG_B(ai) == a &&
              GETARG_B(gi) == a && GETARG_C(gi) == 0 &&
              GETARG_C(ai) < ns && ttisinteger(&base[GETARG_C(ai)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                       &pk, &poff, &pesz)) {
            int da = GETARG_A(gi), ks = GETARG_C(ai);
            if (da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (!seen[ks]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[ks] = 1;
            }
            in.kind = pk == 2 ? LUAR_IR_CPGETF :
                      (pesz == 8 ? LUAR_IR_CPGETI64 : LUAR_IR_CPGETI);
            in.a = (uint8_t)da; in.b = (uint8_t)b; in.c = (uint8_t)ks;
            in.ival = (int64_t)poff; in.nval = (double)pesz;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 3;
            break;
          }
          if (GET_OPCODE(ai) == OP_ADDI &&
              GET_OPCODE(mi) == OP_MMBINI &&
              GET_OPCODE(gi) == OP_GETTABLE &&
              GETARG_B(gi) == a && GETARG_A(ai) == GETARG_C(gi) &&
              GETARG_B(ai) < ns && ttisinteger(&base[GETARG_B(ai)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                       &pk, &poff, &pesz)) {
            int dest = GETARG_A(ai), da = GETARG_A(gi);
            int ks = GETARG_B(ai);
            if (dest >= ns || da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (!seen[ks]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[ks] = 1;
            }
            in.kind = LUAR_IR_ADDI; in.a = (uint8_t)dest;
            in.b = (uint8_t)ks; in.ival = GETARG_sC(ai); in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[dest] = 1;
            in.kind = pk == 2 ? LUAR_IR_CPGETF :
                      (pesz == 8 ? LUAR_IR_CPGETI64 : LUAR_IR_CPGETI);
            in.a = (uint8_t)da; in.b = (uint8_t)b; in.c = (uint8_t)dest;
            in.ival = (int64_t)poff; in.nval = (double)pesz;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 3;
            break;
          }
        }
        if (pc + 2 <= backedge) {
          Instruction ai = p->code[pc + 1], gi = p->code[pc + 2];
          size_t poff = 0, pesz = 0;
          int pk = 0;
          if (GET_OPCODE(ai) == OP_ADDI &&
              GET_OPCODE(gi) == OP_GETTABLE &&
              GETARG_B(gi) == a && GETARG_A(ai) == GETARG_C(gi) &&
              GETARG_B(ai) < ns && ttisinteger(&base[GETARG_B(ai)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                       &pk, &poff, &pesz)) {
            int dest = GETARG_A(ai), da = GETARG_A(gi);
            int ks = GETARG_B(ai);
            if (dest >= ns || da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (!seen[ks]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)ks;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[ks] = 1;
            }
            in.kind = LUAR_IR_ADDI; in.a = (uint8_t)dest;
            in.b = (uint8_t)ks; in.ival = GETARG_sC(ai); in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[dest] = 1;
            in.kind = pk == 2 ? LUAR_IR_CPGETF :
                      (pesz == 8 ? LUAR_IR_CPGETI64 : LUAR_IR_CPGETI);
            in.a = (uint8_t)da; in.b = (uint8_t)b; in.c = (uint8_t)dest;
            in.ival = (int64_t)poff; in.nval = (double)pesz;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 2;
            break;
          }
        }
        if (pc + 1 <= backedge) {
          Instruction nxi = p->code[pc + 1];
          size_t poff = 0, pesz = 0;
          int pk = 0;
          if (GET_OPCODE(nxi) == OP_SETTABLE && GETARG_A(nxi) == a &&
              ttisinteger(&base[GETARG_B(nxi)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                       &pk, &poff, &pesz)) {
            int kc = GETARG_B(nxi);
            int vc = GETARG_C(nxi);
            TValue *v = &base[vc];
            if (pk == 1 ? !ttisinteger(v) : !ttisfloat(v)) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (!seen[kc]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)kc;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[kc] = 1;
            }
            if (!seen[vc]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vc;
              in.b = (uint8_t)(pk == 2 ? LUAR_IR_TAG_FLOAT : LUAR_IR_TAG_INT);
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[vc] = 1;
            }
            in.kind = pk == 2 ? LUAR_IR_CPSETF :
                      (pesz == 8 ? LUAR_IR_CPSETI64 : LUAR_IR_CPSETI);
            in.a = (uint8_t)b; in.b = (uint8_t)kc; in.c = (uint8_t)vc;
            in.ival = (int64_t)poff;
            in.nval = (double)pesz;
            in.pc = pc;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc++;
            break;
          }
          if (GET_OPCODE(nxi) == OP_GETTABLE && GETARG_B(nxi) == a &&
              GETARG_C(nxi) < ns && ttisinteger(&base[GETARG_C(nxi)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_ptr_field(&base[b], getstr(tsvalue(&p->k[c])),
                                       &pk, &poff, &pesz)) {
            int da = GETARG_A(nxi);
            int kc = GETARG_C(nxi);
            if (da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (!seen[kc]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)kc;
              in.b = LUAR_IR_TAG_INT; in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[kc] = 1;
            }
            in.kind = pk == 2 ? LUAR_IR_CPGETF :
                      (pesz == 8 ? LUAR_IR_CPGETI64 : LUAR_IR_CPGETI);
            in.a = (uint8_t)da; in.b = (uint8_t)b; in.c = (uint8_t)kc;
            in.ival = (int64_t)poff;
            in.nval = (double)pesz;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc++;
            break;
          }
        }
        if (pc + 1 <= backedge && luar_ffi_trace_kind(&base[b]) == 3) {
          const char *names[8];
          int chain = 1, cur = a, pp = pc;
          names[0] = getstr(tsvalue(&p->k[c]));
          while (chain < 8 && pp + 1 <= backedge) {
            Instruction nx = p->code[pp + 1];
            int kc;
            if (GET_OPCODE(nx) != OP_GETFIELD || GETARG_B(nx) != cur)
              break;
            kc = GETARG_C(nx);
            if (kc >= p->sizek || !ttisstring(&p->k[kc]))
              break;
            names[chain] = getstr(tsvalue(&p->k[kc]));
            cur = GETARG_A(nx);
            chain++;
            pp++;
          }
          if (chain >= 2 &&
              luar_ffi_trace_fieldn(&base[b], names, chain, &fk, &off,
                                    &fsize)) {
            if (cur >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            in.kind = LUAR_IR_CGETFIELD; in.a = (uint8_t)cur;
            in.b = (uint8_t)b; in.c = (uint8_t)fk;
            in.ival = (int64_t)off;
            in.nval = (double)fsize;
            in.pc = pc;
            seen[cur] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += chain - 1;
            break;
          }
        }
        if (pc + 1 <= backedge && luar_ffi_trace_kind(&base[b]) == 3) {
          const char *names[8];
          int chain = 1, cur = a, pp = pc;
          names[0] = getstr(tsvalue(&p->k[c]));
          while (chain < 8 && pp + 1 <= backedge) {
            Instruction nx = p->code[pp + 1];
            if (GET_OPCODE(nx) == OP_GETFIELD && GETARG_B(nx) == cur) {
              int kc = GETARG_C(nx);
              if (kc >= p->sizek || !ttisstring(&p->k[kc]))
                break;
              names[chain] = getstr(tsvalue(&p->k[kc]));
              cur = GETARG_A(nx);
              chain++;
              pp++;
              continue;
            }
            if (GET_OPCODE(nx) == OP_SETFIELD && GETARG_A(nx) == cur) {
              int kc = GETARG_B(nx), isk = GETARG_k(nx), vc = GETARG_C(nx);
              TValue *v = isk ? &p->k[vc] : &base[vc];
              if (kc >= p->sizek || !ttisstring(&p->k[kc]))
                break;
              names[chain] = getstr(tsvalue(&p->k[kc]));
              if (!luar_ffi_trace_fieldn(&base[b], names, chain + 1,
                                         &fk, &off, &fsize))
                break;
              if (fk == 1 ? !ttisinteger(v) : !ttisfloat(v))
                return 0;
              if (!seen[b]) {
                in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
                in.ival = (int64_t)(intptr_t)base[b].value_.gc;
                in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[b] = 1;
              }
              if (isk) {
                in.kind = LUAR_IR_CSETFIELDK; in.a = (uint8_t)b;
                in.b = (uint8_t)fsize; in.c = (uint8_t)fk;
                in.ival = (int64_t)off;
                in.nval = fk == 1 ? (double)ivalue(v) : fltvalue(v);
                in.pc = pc;
                r = luar_trace_ir_emit(t, &in);
                if (r) pc += chain;
                break;
              }
              if (!seen[vc]) {
                in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vc;
                in.b = (uint8_t)(fk == 2 ? LUAR_IR_TAG_FLOAT :
                                          LUAR_IR_TAG_INT);
                in.pc = pc;
                if (!luar_trace_ir_emit(t, &in)) return 0;
                seen[vc] = 1;
              }
              in.kind = LUAR_IR_CSETFIELD; in.a = (uint8_t)b;
              in.b = (uint8_t)vc; in.c = (uint8_t)fk;
              in.ival = (int64_t)off;
              in.nval = (double)fsize;
              in.pc = pc;
              r = luar_trace_ir_emit(t, &in);
              if (r) pc += chain;
              break;
            }
            break;
          }
          if (r == 1 && pc > pp)
            break;
        }
        if (pc + 3 <= backedge) {
          Instruction q1 = p->code[pc + 1], q2 = p->code[pc + 2];
          Instruction q3 = p->code[pc + 3];
          if (GET_OPCODE(q1) == OP_GETFIELD && GETARG_B(q1) == a &&
              GETARG_C(q1) < p->sizek && ttisstring(&p->k[GETARG_C(q1)]) &&
              GET_OPCODE(q2) == OP_GETFIELD &&
              GETARG_B(q2) == GETARG_A(q1) &&
              GETARG_C(q2) < p->sizek && ttisstring(&p->k[GETARG_C(q2)]) &&
              GET_OPCODE(q3) == OP_SETFIELD &&
              GETARG_A(q3) == GETARG_A(q2) &&
              GETARG_B(q3) < p->sizek && ttisstring(&p->k[GETARG_B(q3)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_field4(&base[b], getstr(tsvalue(&p->k[c])),
                                    getstr(tsvalue(&p->k[GETARG_C(q1)])),
                                    getstr(tsvalue(&p->k[GETARG_C(q2)])),
                                    getstr(tsvalue(&p->k[GETARG_B(q3)])),
                                    &fk, &off, &fsize)) {
            int isk = GETARG_k(q3);
            int vc = GETARG_C(q3);
            TValue *v = isk ? &p->k[vc] : &base[vc];
            if (fk == 1 ? !ttisinteger(v) : !ttisfloat(v)) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (isk) {
              in.kind = LUAR_IR_CSETFIELDK; in.a = (uint8_t)b;
              in.b = (uint8_t)fsize; in.c = (uint8_t)fk;
              in.ival = (int64_t)off;
              in.nval = fk == 1 ? (double)ivalue(v) : fltvalue(v);
              in.pc = pc;
              r = luar_trace_ir_emit(t, &in);
              if (r) pc += 3;
              break;
            }
            if (!seen[vc]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vc;
              in.b = (uint8_t)(fk == 2 ? LUAR_IR_TAG_FLOAT : LUAR_IR_TAG_INT);
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[vc] = 1;
            }
            in.kind = LUAR_IR_CSETFIELD; in.a = (uint8_t)b;
            in.b = (uint8_t)vc; in.c = (uint8_t)fk;
            in.ival = (int64_t)off;
            in.nval = (double)fsize;
            in.pc = pc;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 3;
            break;
          }
          if (GET_OPCODE(q1) == OP_GETFIELD && GETARG_B(q1) == a &&
              GETARG_C(q1) < p->sizek && ttisstring(&p->k[GETARG_C(q1)]) &&
              GET_OPCODE(q2) == OP_GETFIELD &&
              GETARG_B(q2) == GETARG_A(q1) &&
              GETARG_C(q2) < p->sizek && ttisstring(&p->k[GETARG_C(q2)]) &&
              GET_OPCODE(q3) == OP_GETFIELD &&
              GETARG_B(q3) == GETARG_A(q2) &&
              GETARG_C(q3) < p->sizek && ttisstring(&p->k[GETARG_C(q3)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_field4(&base[b], getstr(tsvalue(&p->k[c])),
                                    getstr(tsvalue(&p->k[GETARG_C(q1)])),
                                    getstr(tsvalue(&p->k[GETARG_C(q2)])),
                                    getstr(tsvalue(&p->k[GETARG_C(q3)])),
                                    &fk, &off, &fsize)) {
            int da = GETARG_A(q3);
            if (da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            in.kind = LUAR_IR_CGETFIELD; in.a = (uint8_t)da;
            in.b = (uint8_t)b; in.c = (uint8_t)fk;
            in.ival = (int64_t)off;
            in.nval = (double)fsize;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 3;
            break;
          }
        }
        if (pc + 2 <= backedge) {
          Instruction n1i = p->code[pc + 1], n2i = p->code[pc + 2];
          if (GET_OPCODE(n1i) == OP_GETFIELD &&
              GETARG_B(n1i) == a &&
              GETARG_C(n1i) < p->sizek &&
              ttisstring(&p->k[GETARG_C(n1i)]) &&
              GET_OPCODE(n2i) == OP_SETFIELD &&
              GETARG_A(n2i) == GETARG_A(n1i) &&
              GETARG_B(n2i) < p->sizek &&
              ttisstring(&p->k[GETARG_B(n2i)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_field3(&base[b], getstr(tsvalue(&p->k[c])),
                                    getstr(tsvalue(&p->k[GETARG_C(n1i)])),
                                    getstr(tsvalue(&p->k[GETARG_B(n2i)])),
                                    &fk, &off, &fsize)) {
            int isk = GETARG_k(n2i);
            int vc = GETARG_C(n2i);
            TValue *v = isk ? &p->k[vc] : &base[vc];
            if (fk == 1 ? !ttisinteger(v) : !ttisfloat(v)) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (isk) {
              in.kind = LUAR_IR_CSETFIELDK; in.a = (uint8_t)b;
              in.b = (uint8_t)fsize; in.c = (uint8_t)fk;
              in.ival = (int64_t)off;
              in.nval = fk == 1 ? (double)ivalue(v) : fltvalue(v);
              in.pc = pc;
              r = luar_trace_ir_emit(t, &in);
              if (r) pc += 2;
              break;
            }
            if (!seen[vc]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vc;
              in.b = (uint8_t)(fk == 2 ? LUAR_IR_TAG_FLOAT : LUAR_IR_TAG_INT);
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[vc] = 1;
            }
            in.kind = LUAR_IR_CSETFIELD; in.a = (uint8_t)b;
            in.b = (uint8_t)vc; in.c = (uint8_t)fk;
            in.ival = (int64_t)off;
            in.nval = (double)fsize;
            in.pc = pc;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 2;
            break;
          }
          if (GET_OPCODE(n1i) == OP_GETFIELD &&
              GETARG_B(n1i) == a &&
              GETARG_C(n1i) < p->sizek &&
              ttisstring(&p->k[GETARG_C(n1i)]) &&
              GET_OPCODE(n2i) == OP_GETFIELD &&
              GETARG_B(n2i) == GETARG_A(n1i) &&
              GETARG_C(n2i) < p->sizek &&
              ttisstring(&p->k[GETARG_C(n2i)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_field3(&base[b], getstr(tsvalue(&p->k[c])),
                                    getstr(tsvalue(&p->k[GETARG_C(n1i)])),
                                    getstr(tsvalue(&p->k[GETARG_C(n2i)])),
                                    &fk, &off, &fsize)) {
            int da = GETARG_A(n2i);
            if (da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            in.kind = LUAR_IR_CGETFIELD; in.a = (uint8_t)da;
            in.b = (uint8_t)b; in.c = (uint8_t)fk;
            in.ival = (int64_t)off;
            in.nval = (double)fsize;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc += 2;
            break;
          }
        }
        if (pc + 1 <= backedge) {
          Instruction nx = p->code[pc + 1];
          if (GET_OPCODE(nx) == OP_SETFIELD && GETARG_A(nx) == a &&
              GETARG_B(nx) < p->sizek &&
              ttisstring(&p->k[GETARG_B(nx)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_field2(&base[b], getstr(tsvalue(&p->k[c])),
                                    getstr(tsvalue(&p->k[GETARG_B(nx)])),
                                    &fk, &off, &fsize)) {
            int isk = GETARG_k(nx);
            int vc = GETARG_C(nx);
            TValue *v = isk ? &p->k[vc] : &base[vc];
            if (fk == 1 ? !ttisinteger(v) : !ttisfloat(v)) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            if (isk) {
              in.kind = LUAR_IR_CSETFIELDK; in.a = (uint8_t)b;
              in.b = (uint8_t)fsize; in.c = (uint8_t)fk;
              in.ival = (int64_t)off;
              in.nval = fk == 1 ? (double)ivalue(v) : fltvalue(v);
              in.pc = pc;
              r = luar_trace_ir_emit(t, &in);
              if (r) pc++;
              break;
            }
            if (!seen[vc]) {
              in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vc;
              in.b = (uint8_t)(fk == 2 ? LUAR_IR_TAG_FLOAT : LUAR_IR_TAG_INT);
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[vc] = 1;
            }
            in.kind = LUAR_IR_CSETFIELD; in.a = (uint8_t)b;
            in.b = (uint8_t)vc; in.c = (uint8_t)fk;
            in.ival = (int64_t)off;
            in.nval = (double)fsize;
            in.pc = pc;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc++;
            break;
          }
          if (GET_OPCODE(nx) == OP_GETFIELD && GETARG_B(nx) == a &&
              GETARG_C(nx) < p->sizek &&
              ttisstring(&p->k[GETARG_C(nx)]) &&
              luar_ffi_trace_kind(&base[b]) == 3 &&
              luar_ffi_trace_field2(&base[b], getstr(tsvalue(&p->k[c])),
                                    getstr(tsvalue(&p->k[GETARG_C(nx)])),
                                    &fk, &off, &fsize)) {
            int da = GETARG_A(nx);
            if (da >= ns) return 0;
            if (!seen[b]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
              in.ival = (int64_t)(intptr_t)base[b].value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[b] = 1;
            }
            in.kind = LUAR_IR_CGETFIELD; in.a = (uint8_t)da;
            in.b = (uint8_t)b; in.c = (uint8_t)fk;
            in.ival = (int64_t)off;
            in.nval = (double)fsize;
            in.pc = pc;
            seen[da] = 1;
            r = luar_trace_ir_emit(t, &in);
            if (r) pc++;
            break;
          }
        }
        {
          int tracked = ir_latest_writer_is(t, b, LUAR_IR_ULOAD) &&
                        known_cval[b].tt_ != 0;
          TValue *cdv = tracked ? &known_cval[b] : &base[b];
          if (luar_ffi_trace_kind(cdv) > 0 &&
              luar_ffi_trace_field(cdv, getstr(tsvalue(&p->k[c])),
                                   &fk, &off, &fsize)) {
          if (!seen[b] || tracked) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
            in.ival = (int64_t)(intptr_t)cdv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[b] = 1;
          }
          in.kind = LUAR_IR_CGETFIELD; in.a = (uint8_t)a;
          in.b = (uint8_t)b; in.c = (uint8_t)fk;
          in.ival = (int64_t)off;
          in.nval = (double)fsize;
          in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
          }
        }
        return 0;
      }
      case OP_GETTABLE: {
        int tracked = ir_latest_writer_is(t, b, LUAR_IR_ULOAD) &&
                      known_cval[b].tt_ != 0;
        TValue *cdv = tracked ? &known_cval[b] : &base[b];
        int fk = luar_ffi_trace_kind(cdv);
        if (fk > 0) {
          if (!seen[b] || tracked) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
            in.ival = (int64_t)(intptr_t)cdv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[b] = 1;
          }
          if (!seen[c]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)c;
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[c] = 1;
          }
          in.kind = fk == 2 ? LUAR_IR_CGETF : LUAR_IR_CGETI;
          in.a = (uint8_t)a; in.b = (uint8_t)b;
          in.c = (uint8_t)c; in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!seen[b]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)b;
          in.b = (uint8_t)ir_tag(&base[b]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[b] = 1;
        }
        if (!seen[c]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)c;
          in.b = (uint8_t)ir_tag(&base[c]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[c] = 1;
        }
        in.kind = LUAR_IR_TGET; in.a = (uint8_t)a; in.b = (uint8_t)b;
        in.c = (uint8_t)c; in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_SETTABUP: {
        int idx = a;
        int kc = b, kv = c;
        if (idx < 0 || kc < 0 || kc >= p->sizek || !ttisstring(&p->k[kc]))
          return 0;
        if (!TESTARG_k(ins)) {
          if (c >= ns || ttisnil(&base[c]))
            return 0;
          in.kind = LUAR_IR_TSETUPKV; in.a = (uint8_t)idx;
          in.b = (uint8_t)c;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[kc]); in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (kv >= p->sizek)
          return 0;
        if (ttisnil(&p->k[kv])) {
          in.kind = LUAR_IR_TSETUPKN; in.a = (uint8_t)idx;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[kc]); in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (ttisfloat(&p->k[kv])) {
          in.kind = LUAR_IR_TSETUPKF; in.a = (uint8_t)idx;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[kc]);
          in.nval = fltvalue(&p->k[kv]);
        }
        else if (ttisinteger(&p->k[kv])) {
          uint64_t bits;
          int64_t iv = (int64_t)ivalue(&p->k[kv]);
          memcpy(&bits, &iv, sizeof(bits));
          in.kind = LUAR_IR_TSETUPKI; in.a = (uint8_t)idx;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[kc]);
          memcpy(&in.nval, &bits, sizeof(bits));
        }
        else if (ttisboolean(&p->k[kv])) {
          in.kind = LUAR_IR_TSETUPKB; in.a = (uint8_t)idx;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[kc]);
          in.nval = ttistrue(&p->k[kv]) ? 1.0 : 0.0;
        }
        else if (ttisstring(&p->k[kv])) {
          uint64_t bits;
          TString *vs = tsvalue(&p->k[kv]);
          memcpy(&bits, &vs, sizeof(bits));
          in.kind = LUAR_IR_TSETUPKS; in.a = (uint8_t)idx;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[kc]);
          memcpy(&in.nval, &bits, sizeof(bits));
        }
        else
          return 0;
        in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_SETFIELD: {
        int fk, isk = GETARG_k(ins);
        size_t off = 0, fsize = 0;
        TValue *v = isk ? &p->k[c] : &base[c];
        if (!ttisstring(&p->k[b])) return 0;
        if (luar_ffi_trace_kind(&base[a]) == 3 &&
            luar_ffi_trace_field(&base[a], getstr(tsvalue(&p->k[b])),
                                 &fk, &off, &fsize) &&
            (fk == 1 ? ttisinteger(v) : ttisfloat(v))) {
          if (!seen[a]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)a;
            in.ival = (int64_t)(intptr_t)base[a].value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          if (isk) {
            in.kind = LUAR_IR_CSETFIELDK; in.a = (uint8_t)a;
            in.b = (uint8_t)fsize; in.c = (uint8_t)fk;
            in.ival = (int64_t)off;
            in.nval = fk == 1 ? (double)ivalue(v) : fltvalue(v);
            in.pc = pc;
            r = luar_trace_ir_emit(t, &in);
            break;
          }
          if (!seen[c]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)c;
            in.b = (uint8_t)(fk == 2 ? LUAR_IR_TAG_FLOAT : LUAR_IR_TAG_INT);
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[c] = 1;
          }
          in.kind = LUAR_IR_CSETFIELD; in.a = (uint8_t)a;
          in.b = (uint8_t)c; in.c = (uint8_t)fk;
          in.ival = (int64_t)off;
          in.nval = (double)fsize;
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (isk && ttisinteger(v)) {
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          in.kind = LUAR_IR_TSETKK; in.a = (uint8_t)a;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          in.nval = (double)ivalue(v); in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (isk && ttisfloat(v)) {
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          in.kind = LUAR_IR_TSETKKF; in.a = (uint8_t)a;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          in.nval = fltvalue(v); in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (isk && ttisstring(v)) {
          uint64_t bits;
          TString *vs = tsvalue(v);
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          memcpy(&bits, &vs, sizeof(bits));
          in.kind = LUAR_IR_TSETKKS; in.a = (uint8_t)a;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          memcpy(&in.nval, &bits, sizeof(bits));
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!isk && ttisinteger(v)) {
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          if (!seen[c]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)c;
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[c] = 1;
          }
          in.kind = LUAR_IR_TSETK; in.a = (uint8_t)a;
          in.b = (uint8_t)c;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (isk && ttisboolean(v)) {
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          in.kind = LUAR_IR_TSETKB; in.a = (uint8_t)a;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          in.nval = ttistrue(v) ? 1.0 : 0.0; in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!isk && ttisboolean(v)) {
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          in.kind = LUAR_IR_TSETKDB; in.a = (uint8_t)a;
          in.b = (uint8_t)c;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!isk && ttistable(v)) {
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          in.kind = LUAR_IR_TSETKT; in.a = (uint8_t)a;
          in.b = (uint8_t)c;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!isk && ttisstring(v)) {
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          in.kind = LUAR_IR_TSETKS; in.a = (uint8_t)a;
          in.b = (uint8_t)c;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!isk && ttisfloat(v)) {
          if (!seen[a]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
            in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          if (!seen[c]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)c;
            in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[c] = 1;
          }
          in.kind = LUAR_IR_TSETKF; in.a = (uint8_t)a;
          in.b = (uint8_t)c;
          in.ival = (int64_t)(intptr_t)tsvalue(&p->k[b]);
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        return 0;
      }
      case OP_SETI: {
        int key = GETARG_B(ins), vs = GETARG_C(ins);
        if (key < 0 || vs >= ns || GETARG_k(ins))
          return 0;
        if (!ttisinteger(&base[vs]) &&
            ir_find_arg_int(p, pc - 1, vs, base) < 0)
          return 0;
        if (!seen[a]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
          in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[a] = 1;
        }
        if (!seen[vs]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vs;
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[vs] = 1;
        }
        in.kind = LUAR_IR_TSETI; in.a = (uint8_t)a;
        in.b = (uint8_t)vs; in.ival = key; in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_GETI: {
        int key = GETARG_C(ins);
        if (b >= ns || key < 0)
          return 0;
        if (!seen[b]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)b;
          in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[b] = 1;
        }
        in.kind = LUAR_IR_TGETI; in.a = (uint8_t)a;
        in.b = (uint8_t)b; in.ival = key; in.pc = pc;
        seen[a] = 1;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_SETTABLE: {
        if (ttistable(&base[a]) &&
            ((Table *)base[a].value_.gc)->metatable == NULL &&
            TESTARG_k(ins) && b < ns && c < p->sizek &&
            ttisboolean(&p->k[c])) {
          in.kind = LUAR_IR_TSETDYN_KB; in.a = (uint8_t)a;
          in.b = (uint8_t)b;
          in.ival = ttistrue(&p->k[c]) ? 1 : 0; in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (ttistable(&base[a]) &&
            ((Table *)base[a].value_.gc)->metatable == NULL &&
            TESTARG_k(ins) && b < ns && c < p->sizek &&
            ttisinteger(&p->k[c])) {
          uint64_t bits;
          int64_t iv = (int64_t)ivalue(&p->k[c]);
          memcpy(&bits, &iv, sizeof(bits));
          in.kind = LUAR_IR_TSETDYN_KI; in.a = (uint8_t)a;
          in.b = (uint8_t)b; memcpy(&in.nval, &bits, sizeof(bits));
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (ttistable(&base[a]) &&
            ((Table *)base[a].value_.gc)->metatable == NULL &&
            TESTARG_k(ins) && b < ns && c < p->sizek &&
            ttisstring(&p->k[c])) {
          uint64_t bits;
          TString *vs = tsvalue(&p->k[c]);
          memcpy(&bits, &vs, sizeof(bits));
          in.kind = LUAR_IR_TSETDYN_KS; in.a = (uint8_t)a;
          in.b = (uint8_t)b; memcpy(&in.nval, &bits, sizeof(bits));
          in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (ttistable(&base[a]) &&
            ((Table *)base[a].value_.gc)->metatable == NULL &&
            TESTARG_k(ins) && b < ns && c < p->sizek &&
            ttisfloat(&p->k[c])) {
          in.kind = LUAR_IR_TSETDYN_KF; in.a = (uint8_t)a;
          in.b = (uint8_t)b; in.nval = fltvalue(&p->k[c]); in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (ttistable(&base[a]) &&
            ((Table *)base[a].value_.gc)->metatable == NULL &&
            b < ns && c < ns &&
            (ttisfloat(&base[c]) || ttistable(&base[c]) ||
             ttisstring(&base[c]) || ttisboolean(&base[c]) ||
             (ttisinteger(&base[c]) &&
              (((Table *)base[a].value_.gc)->asize > 0 ||
               ((Table *)base[a].value_.gc)->node != NULL)))) {
          in.kind = LUAR_IR_TSETDYN; in.a = (uint8_t)a;
          in.b = (uint8_t)b; in.c = (uint8_t)c; in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        int fk = luar_ffi_trace_kind(&base[a]);
        if (fk > 0 && ttisinteger(&base[b]) &&
            (fk == 1 ? ttisinteger(&base[c]) : ttisfloat(&base[c]))) {
          if (!seen[a]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)a;
            in.ival = (int64_t)(intptr_t)base[a].value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a] = 1;
          }
          if (!seen[b]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)b;
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[b] = 1;
          }
          if (!seen[c]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)c;
            in.b = (uint8_t)(fk == 2 ? LUAR_IR_TAG_FLOAT : LUAR_IR_TAG_INT);
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[c] = 1;
          }
          in.kind = fk == 2 ? LUAR_IR_CSETF : LUAR_IR_CSETI;
          in.a = (uint8_t)a; in.b = (uint8_t)b;
          in.c = (uint8_t)c; in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!seen[a]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
          in.b = (uint8_t)ir_tag(&base[a]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[a] = 1;
        }
        if (!seen[b]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)b;
          in.b = (uint8_t)ir_tag(&base[b]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[b] = 1;
        }
        if (!seen[c]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)c;
          in.b = (uint8_t)ir_tag(&base[c]); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[c] = 1;
        }
        in.kind = LUAR_IR_TSET; in.a = (uint8_t)a; in.b = (uint8_t)b;
        in.c = (uint8_t)c; in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_MMBIN:
      case OP_MMBINI:
      case OP_MMBINK:
        r = 1;
        break;
      case OP_NEWTABLE: {
        unsigned nb = GETARG_vB(ins), nc = GETARG_vC(ins);
        if (suffix || partial || TESTARG_k(ins))
          return 0;
        in.kind = LUAR_IR_TNEW; in.a = (uint8_t)a;
        in.ival = (int64_t)nc;
        in.nval = nb > 0 ? (double)(1u << (nb - 1)) : 0.0;
        in.pc = pc;
        seen[a] = 1;
        r = luar_trace_ir_emit(t, &in);
        if (r) pc++;   /* skip the paired EXTRAARG */
        break;
      }
      case OP_SETLIST: {
        int n = GETARG_vB(ins), last = GETARG_vC(ins);
        int j;
        if (suffix || partial || n <= 0 || a + n >= ns)
          return 0;
        if (TESTARG_k(ins)) {
          if (pc + 1 > backedge ||
              GET_OPCODE(p->code[pc + 1]) != OP_EXTRAARG)
            return 0;
          last = GETARG_Ax(p->code[pc + 1]);
          pc++;
        }
        if (!seen[a]) {
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)a;
          in.b = LUAR_IR_TAG_TABLE; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          seen[a] = 1;
        }
        last += n;
        for (j = 1; j <= n; j++) {
          int vs = a + j;
          int vk;
          if (ttisinteger(&base[vs]))
            vk = LUAR_IR_TSETI;
          else if (ttisfloat(&base[vs]))
            vk = LUAR_IR_TSETIF;
          else if (ttisstring(&base[vs]))
            vk = LUAR_IR_TSETIS;
          else if (ttisboolean(&base[vs]))
            vk = LUAR_IR_TSETIB;
          else if (ttistable(&base[vs]))
            vk = LUAR_IR_TSETIT;
          else
            return 0;
          if (vk == LUAR_IR_TSETI && !seen[vs]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)vs;
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[vs] = 1;
          }
          in.kind = (uint8_t)vk; in.a = (uint8_t)a;
          in.b = (uint8_t)vs; in.ival = last - n + j; in.pc = pc;
          r = luar_trace_ir_emit(t, &in);
          if (!r) return 0;
        }
        break;
      }
      case OP_EXTRAARG:
        r = 1;
        break;
      case OP_LEN: {
        if (b >= ns)
          return 0;
        if (ttistable(&base[b])) {
          in.kind = LUAR_IR_TLEN; in.a = (uint8_t)a;
          in.b = (uint8_t)b; in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (!ttisstring(&base[b]))
          return 0;
        in.kind = LUAR_IR_STRLEN; in.a = (uint8_t)a;
        in.b = (uint8_t)b; in.pc = pc;
        seen[a] = 1;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_CONCAT: {
        int n = GETARG_B(ins);
        if (n < 2 || n > 32 || a + n > ns)
          return 0;
        in.kind = (n == 2) ? LUAR_IR_CONCAT2 :
                  (n == 3) ? LUAR_IR_CONCAT3 : LUAR_IR_CONCATN;
        in.a = (uint8_t)a; in.ival = n; in.pc = pc;
        seen[a] = 1;
        r = luar_trace_ir_emit(t, &in);
        break;
      }
      case OP_CALL: {
        int mark = t->nins;
        int64_t iv1 = 0, iv2 = 0;
        int fs = fn_src[a] >= 0 ? fn_src[a] : a;
        int use_known = known[a] &&
                        ir_latest_writer_is(t, a, LUAR_IR_ULOAD);
        TValue *fv = use_known ? &known_fn[a] : &base[fs];
        TValue *fn = fv;
        int inline_ok = (c == 2 && (b == 2 || b == 3) &&
                         a + b - 1 < ns && ttisLclosure(fv) &&
                         ttisinteger(&base[a + 1]) &&
                         (b == 2 || ttisinteger(&base[a + 2])));
        if (b == 1 && c == 2 && luar_ffi_trace_ccall0_kind(fv) == 2) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_CCALL0D; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 1 && c == 2 && luar_ffi_trace_ccall0_kind(fv) == 1) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_CCALL0I; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 2 && c == 2 && a + 1 < ns &&
            ir_call_arg_is_int(p, pc, a + 1, base) &&
            !ir_last_op_writes_float(t, a + 1) &&
            luar_ffi_trace_ccall1di_kind(fv) == 1) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL1DI; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1); in.pc = pc;
          seen[a] = 1;
          in.kind = LUAR_IR_CCALL1DI; /* keep op */
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 2 && c == 2 && a + 1 < ns &&
            luar_ffi_trace_ccall1dd_kind(fv) == 1 &&
            ir_slot_is_int(t, a + 1, t->nins, 0)) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_ITOF; in.a = (uint8_t)(a + 1);
          in.b = (uint8_t)(a + 1); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL1DD; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1); in.pc = pc;
          seen[a] = 1; seen[a + 1] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 2 && c == 2 && a + 1 < ns &&
            (ir_call_arg_is_double(p, pc, a + 1, base) ||
             ir_last_op_writes_float(t, a + 1)) &&
            luar_ffi_trace_ccall1dd_kind(fv) == 1) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          /* The argument producer (MOVE/LOADK or the CGETF/CPGETF/
          ** CGETFIELD last op) has already executed here, so this guard
          ** checks the value the helper is about to read. */
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL1DD; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1); in.pc = pc;
          seen[a] = 1;
          seen[a + 1] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2ii_kind(fv) == 1) {
          int q2 = ir_find_arg_int(p, pc - 1, a + 2, base);
          int q1 = q2 >= 0 ? ir_find_arg_int(p, q2 - 1, a + 1, base) : -1;
          if (q1 >= 0) {
            if (!seen[fs]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
              in.ival = (int64_t)(intptr_t)fv->value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[fs] = 1;
            }
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            in.kind = LUAR_IR_CCALL2II; in.a = (uint8_t)a;
            in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
            in.ival = (int64_t)(a + 2); in.pc = pc;
            seen[a] = 1;
            r = luar_trace_ir_emit(t, &in);
            break;
          }
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2df_kind(fv) == 1 &&
            (ir_find_arg_double(p, pc - 1, a + 2, base) >= 0 ||
             ir_slot_is_float(t, a + 2, t->nins, 0)) &&
            (ir_find_arg_double(p, pc - 2, a + 1, base) >= 0 ||
             ir_slot_is_float(t, a + 1, t->nins, 0) ||
             ir_slot_is_int(t, a + 1, t->nins, 0))) {
          int i1 = ir_slot_is_int(t, a + 1, t->nins, 0);
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          if (i1) {
            in.kind = LUAR_IR_ITOF; in.a = (uint8_t)(a + 1);
            in.b = (uint8_t)(a + 1); in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2DF; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2fd_kind(fv) == 1 &&
            (ir_find_arg_double(p, pc - 1, a + 2, base) >= 0 ||
             ir_slot_is_float(t, a + 2, t->nins, 0) ||
             ir_slot_is_int(t, a + 2, t->nins, 0)) &&
            (ir_find_arg_double(p, pc - 2, a + 1, base) >= 0 ||
             ir_slot_is_float(t, a + 1, t->nins, 0))) {
          int i2 = ir_slot_is_int(t, a + 2, t->nins, 0);
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          if (i2) {
            in.kind = LUAR_IR_ITOF; in.a = (uint8_t)(a + 2);
            in.b = (uint8_t)(a + 2); in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2FD; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2if_kind(fv) == 1 &&
            (ir_call_args_if(p, pc, a + 1, base) ||
             ir_slot_is_float(t, a + 2, t->nins, 0))) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2IF; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2fi_kind(fv) == 1 &&
            (ir_call_args_fi(p, pc, a + 1, base) ||
             ir_slot_is_float(t, a + 1, t->nins, 0))) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2FI; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2id_kind(fv) == 1 &&
            (ir_call_args_id(p, pc, a + 1, base) ||
             ir_slot_is_float(t, a + 2, t->nins, 0) ||
             ir_slot_is_int(t, a + 2, t->nins, 0))) {
          int i2 = ir_slot_is_int(t, a + 2, t->nins, 0);
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          if (i2) {
            in.kind = LUAR_IR_ITOF; in.a = (uint8_t)(a + 2);
            in.b = (uint8_t)(a + 2); in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2ID; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2di_kind(fv) == 1) {
          int q2 = ir_find_arg_int(p, pc - 1, a + 2, base);
          int q1 = -1, q1i = 0;
          if (q2 >= 0) {
            if (ir_find_arg_double(p, q2 - 1, a + 1, base) >= 0 ||
                ir_slot_is_float(t, a + 1, t->nins, 0))
              q1 = 1;
            else if (ir_slot_is_int(t, a + 1, t->nins, 0)) {
              q1 = 1; q1i = 1;
            }
          }
          if (q1 >= 0) {
            if (!seen[fs]) {
              in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
              in.ival = (int64_t)(intptr_t)fv->value_.gc;
              in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
              seen[fs] = 1;
            }
            if (q1i) {
              in.kind = LUAR_IR_ITOF; in.a = (uint8_t)(a + 1);
              in.b = (uint8_t)(a + 1); in.pc = pc;
              if (!luar_trace_ir_emit(t, &in)) return 0;
            }
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
            in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            in.kind = LUAR_IR_CCALL2DI; in.a = (uint8_t)a;
            in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
            in.ival = (int64_t)(a + 2); in.pc = pc;
            seen[a] = 1;
            r = luar_trace_ir_emit(t, &in);
            break;
          }
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2ff_kind(fv) == 1 &&
            ir_slot_const_int(t, a + 1, known_cval, &iv1) &&
            ir_slot_is_float(t, a + 2, t->nins, 0)) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_LOADF; in.a = (uint8_t)(a + 1);
          in.nval = (double)iv1; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2FF; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2ff_kind(fv) == 1 &&
            ir_slot_is_float(t, a + 1, t->nins, 0) &&
            ir_slot_const_int(t, a + 2, known_cval, &iv2)) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_LOADF; in.a = (uint8_t)(a + 2);
          in.nval = (double)iv2; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2FF; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2ff_kind(fv) == 1 &&
            ir_slot_const_int(t, a + 1, known_cval, &iv1) &&
            ir_slot_const_int(t, a + 2, known_cval, &iv2)) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_LOADF; in.a = (uint8_t)(a + 1);
          in.nval = (double)iv1; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_LOADF; in.a = (uint8_t)(a + 2);
          in.nval = (double)iv2; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2FF; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            luar_ffi_trace_ccall2ff_kind(fv) == 1 &&
            (ir_slot_is_int(t, a + 1, t->nins, 0) ||
             ir_slot_is_float(t, a + 1, t->nins, 0)) &&
            (ir_slot_is_int(t, a + 2, t->nins, 0) ||
             ir_slot_is_float(t, a + 2, t->nins, 0))) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_ITOF; in.a = (uint8_t)(a + 1);
          in.b = (uint8_t)(a + 1); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_ITOF; in.a = (uint8_t)(a + 2);
          in.b = (uint8_t)(a + 2); in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2FF; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            (ir_call_args2_are_double(p, pc, a + 1, a + 2, base) ||
             (ir_slot_is_float(t, a + 1, t->nins, 0) &&
              ir_slot_is_float(t, a + 2, t->nins, 0))) &&
            luar_ffi_trace_ccall2ff_kind(fv) == 1) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2FF; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 3 && c == 2 && a + 2 < ns &&
            (ir_call_args2_are_double(p, pc, a + 1, a + 2, base) ||
             (ir_slot_is_float(t, a + 1, t->nins, 0) &&
              ir_slot_is_float(t, a + 2, t->nins, 0))) &&
            luar_ffi_trace_ccall2dd_kind(fv) == 1) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL2DD; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2); in.pc = pc;
          seen[a] = 1;
          seen[a + 1] = 1;
          seen[a + 2] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 4 && c == 2 && a + 3 < ns &&
            luar_ffi_trace_ccall3idd_kind(fv) == 1 &&
            ir_slot_is_float(t, a + 2, t->nins, 0) &&
            ir_slot_is_float(t, a + 3, t->nins, 0)) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 3);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL3IDD; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 2);
          in.ival = (int64_t)(a + 1) | ((int64_t)(a + 3) << 8);
          in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 4 && c == 2 && a + 3 < ns &&
            luar_ffi_trace_ccall3did_kind(fv) == 1 &&
            ir_slot_is_float(t, a + 1, t->nins, 0) &&
            ir_slot_is_float(t, a + 3, t->nins, 0)) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 3);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL3DID; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2) | ((int64_t)(a + 3) << 8);
          in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 4 && c == 2 && a + 3 < ns &&
            luar_ffi_trace_ccall3ddi_kind(fv) == 1 &&
            (ir_call_args_ddi(p, pc, a + 1, base) ||
             (ir_slot_is_float(t, a + 1, t->nins, 0) &&
              ir_slot_is_float(t, a + 2, t->nins, 0)))) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 3);
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL3DDI; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2) | ((int64_t)(a + 3) << 8);
          in.pc = pc;
          seen[a] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 4 && c == 2 && a + 3 < ns &&
            (ir_call_args3_are_double(p, pc, a + 1, a + 2, a + 3, base) ||
             (ir_slot_is_float(t, a + 1, t->nins, 0) &&
              ir_slot_is_float(t, a + 2, t->nins, 0) &&
              ir_slot_is_float(t, a + 3, t->nins, 0))) &&
            luar_ffi_trace_ccall3dd_kind(fv) == 1) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 3);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL3DD; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1);
          in.ival = (int64_t)(a + 2) | ((int64_t)(a + 3) << 8);
          in.pc = pc;
          seen[a] = 1;
          seen[a + 1] = 1;
          seen[a + 2] = 1;
          seen[a + 3] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 2 && c == 2 && a + 1 < ns &&
            (ir_call_arg_is_double(p, pc, a + 1, base) ||
             ir_last_op_writes_float(t, a + 1)) &&
            luar_ffi_trace_ccall1ff_kind(fv) == 1) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_FLOAT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL1FF; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1); in.pc = pc;
          seen[a] = 1;
          seen[a + 1] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (b == 2 && c == 2 && a + 1 < ns &&
            ir_call_arg_is_int(p, pc, a + 1, base) &&
            luar_ffi_trace_ccall1_kind(fv) == 1) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          /* The preceding LOADI/ADDI/MOVE has executed by this point in
          ** the emitted trace; guard the *runtime* argument type before
          ** reading its payload. */
          in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
          in.b = LUAR_IR_TAG_INT; in.pc = pc;
          if (!luar_trace_ir_emit(t, &in)) return 0;
          in.kind = LUAR_IR_CCALL1I; in.a = (uint8_t)a;
          in.b = (uint8_t)fs; in.c = (uint8_t)(a + 1); in.pc = pc;
          seen[a] = 1;
          seen[a + 1] = 1;
          r = luar_trace_ir_emit(t, &in);
          break;
        }
        if (inline_ok) {
          if (!seen[fs]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)fs;
            in.ival = (int64_t)(intptr_t)fv->value_.gc;
            in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[fs] = 1;
          }
          if (!seen[a + 1]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 1);
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a + 1] = 1;
          }
          if (b == 3 && !seen[a + 2]) {
            in.kind = LUAR_IR_GUARD; in.a = (uint8_t)(a + 2);
            in.b = LUAR_IR_TAG_INT; in.pc = pc;
            if (!luar_trace_ir_emit(t, &in)) return 0;
            seen[a + 2] = 1;
          }
          if (b == 2 ? inline_leaf_call(t, a, fn, pc)
                     : inline_leaf_call2(t, a, fn, pc)) {
            seen[a] = 1;
            r = 1;
            break;
          }
        }
        if (!partial || pc <= start || b == 0) {
          return 0;
        }
        t->nins = mark;   /* discard speculative inline guards */
        in.kind = LUAR_IR_CALLEXIT; in.pc = pc;
        if (!luar_trace_ir_emit(t, &in)) return 0;
        t->partial = 1;
        if (callpc != NULL) *callpc = pc;
        seen[a] = 1;
        partial_done = 1;
        r = 1;
        break;
      }
      case OP_FORLOOP:
        in.kind = LUAR_IR_FORLOOP; in.a = (uint8_t)a; in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        sawloop = 1;
        break;
      default:
        return 0;
    }
    if (!r) return 0;
    if (partial_done || GET_OPCODE(ins) == OP_FORLOOP)
      break;
  }
  if (t->partial) {
    in.kind = LUAR_IR_RET; in.pc = backedge + 1;
    return luar_trace_ir_emit(t, &in);
  }
  if (!sawloop)
    return 0;
  if (!luar_trace_ir_snapshot(&ex, backedge + 1, base, ns))
    return 0;
  if (!luar_trace_ir_add_exit(t, &ex))
    return 0;
  in.kind = LUAR_IR_EXIT; in.a = 0; in.pc = backedge + 1;
  if (!luar_trace_ir_emit(t, &in)) return 0;
  in.kind = LUAR_IR_RET; in.pc = backedge + 1;
  if (!luar_trace_ir_emit(t, &in)) return 0;
  return luar_trace_ir_verify(t, NULL, 0);
}


int luar_trace_ir_selftest (void) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  LuaR_IRExitState ex;
  TValue base[4];
  char err[64];
  memset(base, 0, sizeof(base));
  setivalue(&base[0], 41);
  setfltvalue(&base[1], 1.5);
  luar_trace_ir_init(&t, NULL, 10, 4, 1);
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_SLOAD; in.a = 2; in.b = 0; in.pc = 10;
  if (!luar_trace_ir_emit(&t, &in)) return 0;
  in.kind = LUAR_IR_GUARD; in.a = 2; in.b = LUAR_IR_TAG_INT;
  if (!luar_trace_ir_emit(&t, &in)) return 0;
  in.kind = LUAR_IR_ADD; in.a = 3; in.b = 2; in.c = 2;
  if (!luar_trace_ir_emit(&t, &in)) return 0;
  if (!luar_trace_ir_snapshot(&ex, 11, base, 2)) return 0;
  if (!luar_trace_ir_add_exit(&t, &ex)) return 0;
  in.kind = LUAR_IR_EXIT; in.a = 0;
  if (!luar_trace_ir_emit(&t, &in)) return 0;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) return 0;
  if (!luar_trace_ir_verify(&t, err, sizeof(err))) return 0;
  base[0].tt_ = LUA_VNIL;
  if (base[0].tt_ == LUA_VNUMINT) return 0;
  if (!luar_trace_ir_restore(&ex, base)) return 0;
  if (!ttisinteger(&base[0]) || ivalue(&base[0]) != 41) return 0;
  in.kind = LUAR_IR_SLOAD; in.a = 4; in.b = 0;
  if (!luar_trace_ir_emit(&t, &in)) return 0;
  if (luar_trace_ir_verify(&t, err, sizeof(err))) return 0;  /* bad slot */
  return 1;
}
