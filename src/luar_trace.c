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
#include "ltm.h"
#include "luar_trace.h"

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
      case LUAR_IR_GUARD:
        if (!valid_slot(t, in->a) ||
            in->b < LUAR_IR_TAG_INT || in->b > LUAR_IR_TAG_OTHER) {
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
        break;
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


int64_t luar_trace_tset_insert (TValue *tv, lua_Integer key, TValue *val,
                                lua_State *L) {
  Table *t;
  TValue old;
  lu_byte tag;
  if (tv == NULL || !ttistable(tv) || val == NULL || !ttisinteger(val) ||
      key < 1 || L == NULL)
    return 0;
  t = (Table *)tv->value_.gc;
  if ((lua_Unsigned)key <= t->asize) {
    if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX) &&
        tagisempty(*getArrTag(t, (unsigned)(key - 1))))
      return 0;
    *getArrTag(t, (unsigned)(key - 1)) = LUA_VNUMINT;
    getArrVal(t, (unsigned)(key - 1))->i = ivalue(val);
    return 1;
  }
  tag = luaH_getint(t, key, &old);
  if (!tagisempty(tag) && !ttisnil(&old)) {
    luaH_setint(L, t, key, val);
    return 1;
  }
  if (t->metatable != NULL && !checknoTM(t->metatable, TM_NEWINDEX))
    return 0;   /* absent key: __newindex owns the write */
  luaH_setint(L, t, key, val);   /* may grow/rehash */
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
                        LuaR_TraceIR *t, int partial, int *callpc,
                        int suffix);

int luar_trace_record_loop (Proto *p, int start, int backedge, TValue *base,
                            LuaR_TraceIR *t) {
  return record_loop(p, start, backedge, base, t, 0, NULL, 0);
}

int luar_trace_record_call_prefix (Proto *p, int start, int backedge,
                                   TValue *base, LuaR_TraceIR *t,
                                   int *callpc) {
  return record_loop(p, start, backedge, base, t, 1, callpc, 0);
}

int luar_trace_record_suffix (Proto *p, int start, int backedge,
                              TValue *base, LuaR_TraceIR *t) {
  return record_loop(p, start, backedge, base, t, 0, NULL, 1);
}


static int abstract_tag (const TValue *v) {
  if (ttisinteger(v)) return LUAR_IR_TAG_INT;
  if (ttisfloat(v)) return LUAR_IR_TAG_FLOAT;
  if (ttistable(v)) return LUAR_IR_TAG_TABLE;
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
      case LUAR_IR_SLOAD:
        kind[in->a] = kind[in->b];
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


static int record_loop (Proto *p, int start, int backedge, TValue *base,
                        LuaR_TraceIR *t, int partial, int *callpc,
                        int suffix) {
  int pc, ns = p->maxstacksize;
  int seen[LUAR_IR_MAX_SLOTS];
  int fn_src[LUAR_IR_MAX_SLOTS];
  LuaR_IRIns in;
  LuaR_IRExitState ex;
  int sawloop = 0, partial_done = 0;
  if (callpc != NULL) *callpc = -1;
  if (ns > LUAR_IR_MAX_SLOTS) ns = LUAR_IR_MAX_SLOTS;
  luar_trace_ir_init(t, p, start, ns, 0);
  memset(seen, 0, sizeof(seen));
  memset(fn_src, -1, sizeof(fn_src));
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
                    GET_OPCODE(ins) != OP_MMBINI) ||
        (c >= ns && (GET_OPCODE(ins) == OP_ADD ||
                     GET_OPCODE(ins) == OP_SUB ||
                     GET_OPCODE(ins) == OP_MUL ||
                     GET_OPCODE(ins) == OP_DIV)))
      return 0;
    switch (GET_OPCODE(ins)) {
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
        if (!seen[b] && b < ns) {
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
      case OP_ADDI:
        in.kind = LUAR_IR_ADDI; in.a = (uint8_t)a; in.b = (uint8_t)b;
        in.ival = GETARG_sC(ins); in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        break;
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
      case OP_GETFIELD: {
        int fk;
        size_t off = 0, fsize = 0;
        if (!ttisstring(&p->k[c])) {
          return 0;
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
        if (luar_ffi_trace_kind(&base[b]) > 0 &&
            luar_ffi_trace_field(&base[b], getstr(tsvalue(&p->k[c])),
                                 &fk, &off, &fsize)) {
          if (!seen[b]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
            in.ival = (int64_t)(intptr_t)base[b].value_.gc;
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
        return 0;
      }
      case OP_GETTABLE: {
        int fk = luar_ffi_trace_kind(&base[b]);
        if (fk > 0 && ttisinteger(&base[c])) {
          if (!seen[b]) {
            in.kind = LUAR_IR_CGUARD; in.a = (uint8_t)b;
            in.ival = (int64_t)(intptr_t)base[b].value_.gc;
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
        return 0;
      }
      case OP_SETTABLE: {
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
      case OP_CALL: {
        int mark = t->nins;
        int fs = fn_src[a] >= 0 ? fn_src[a] : a;
        TValue *fv = &base[fs];
        TValue *fn = &base[fs];
        int inline_ok = (c == 2 && (b == 2 || b == 3) &&
                         a + b - 1 < ns && ttisLclosure(fv) &&
                         ttisinteger(&base[a + 1]) &&
                         (b == 2 || ttisinteger(&base[a + 2])));
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
        if (!partial || pc <= start || b == 0)
          return 0;
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
