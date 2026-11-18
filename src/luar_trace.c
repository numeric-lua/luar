/*
** luar_trace.c -- tracing-IR construction, verification and exit snapshots.
*/
#include "lprefix.h"

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lobject.h"
#include "lopcodes.h"
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
      case LUAR_IR_LOADI:
        if (!valid_slot(t, in->a)) {
          if (err != NULL && errlen > 0) snprintf(err, errlen, "bad loadi");
          return 0;
        }
        break;
      case LUAR_IR_ADDI:
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

static int ir_tag (const TValue *v) {
  if (ttisinteger(v)) return LUAR_IR_TAG_INT;
  if (ttisfloat(v)) return LUAR_IR_TAG_FLOAT;
  if (ttistable(v)) return LUAR_IR_TAG_TABLE;
  return LUAR_IR_TAG_OTHER;
}


int luar_trace_record_loop (Proto *p, int start, int backedge, TValue *base,
                            LuaR_TraceIR *t) {
  int pc, ns = p->maxstacksize;
  int seen[LUAR_IR_MAX_SLOTS];
  LuaR_IRIns in;
  LuaR_IRExitState ex;
  int sawloop = 0;
  if (ns > LUAR_IR_MAX_SLOTS) ns = LUAR_IR_MAX_SLOTS;
  luar_trace_ir_init(t, p, start, ns, 0);
  memset(seen, 0, sizeof(seen));
  memset(&in, 0, sizeof(in));
  for (pc = start; pc <= backedge; pc++) {
    Instruction ins = p->code[pc];
    int a = GETARG_A(ins), b = GETARG_B(ins), c = GETARG_C(ins);
    int r;
    if (a >= ns || (b >= ns && GET_OPCODE(ins) != OP_MMBINK) ||
        (c >= ns && (GET_OPCODE(ins) == OP_ADD ||
                     GET_OPCODE(ins) == OP_SUB ||
                     GET_OPCODE(ins) == OP_MUL ||
                     GET_OPCODE(ins) == OP_DIV)))
      return 0;
    switch (GET_OPCODE(ins)) {
      case OP_MOVE:
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
      case OP_MMBIN:
      case OP_MMBINI:
      case OP_MMBINK:
        r = 1;
        break;
      case OP_FORLOOP:
        in.kind = LUAR_IR_FORLOOP; in.a = (uint8_t)a; in.pc = pc;
        r = luar_trace_ir_emit(t, &in);
        sawloop = 1;
        break;
      default:
        return 0;
    }
    if (!r) return 0;
    if (GET_OPCODE(ins) == OP_FORLOOP)
      break;
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
