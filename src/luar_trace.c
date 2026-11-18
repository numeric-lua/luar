/*
** luar_trace.c -- tracing-IR construction, verification and exit snapshots.
*/
#include "lprefix.h"

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lobject.h"
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
  if (t->depth < 0 || t->depth > 8) {
    if (err != NULL && errlen > 0) snprintf(err, errlen, "bad depth");
    return 0;
  }
  for (i = 0; i < t->nins; i++) {
    const LuaR_IRIns *in = &t->ins[i];
    switch (in->kind) {
      case LUAR_IR_KINT:
      case LUAR_IR_KFLOAT:
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
