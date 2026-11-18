/*
** luar_jit.c -- the 'jit' library, the J1 trace recorder and the J2
** native-loop compiler (x86-64 numeric for-loops).
**
** J1 records hot loop backedges and snapshots the loop bytecode (see
** doc/jit-design.md sections 7 and 10).  J2 then emits a guarded native
** loop with DynASM and hands it the remaining iterations; everything the
** whitelist does not cover stays in the interpreter.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luar.h"

#include "ltable.h"
#include "lopcodes.h"
#include "lopnames.h"
#include "luar_emit.h"
#include "luar_jit.h"

static void pushtrace (lua_State *L, const LuaR_Trace *t);
static void pushir (lua_State *L, const LuaR_Trace *t);


/* ---------------------------------------------------------------- */
/* State                                                             */
/* ---------------------------------------------------------------- */

void luari_jit_init (lua_State *L) {
  LuaR_JIT *J;
  if (G(L)->luar_jit != NULL)
    return;
  J = (LuaR_JIT *)malloc(sizeof(LuaR_JIT));
  if (J == NULL)
    return;
  memset(J, 0, sizeof(*J));
  J->epoch = 1;
  J->enabled = 1;
  J->hotloop = LUAR_JIT_HOTCOUNT;
  G(L)->luar_jit = J;
}


static void free_code (LuaR_JIT *J) {
  int i;
  if (J == NULL)
    return;
  for (i = 0; i < J->ncode; i++) {
    if (J->code[i].code != NULL && J->code[i].size != 0)
      munmap(J->code[i].code, J->code[i].size);
    J->code[i].code = NULL;
    J->code[i].size = 0;
  }
  J->ncode = 0;
  J->compiled = 0;
  J->exits = 0;
  J->mcode = 0;
}


void luari_jit_shutdown (lua_State *L) {
  LuaR_JIT *J = G(L)->luar_jit;
  if (J != NULL) {
    free_code(J);
    G(L)->luar_jit = NULL;
    free(J);
  }
}


/* ---------------------------------------------------------------- */
/* J2: native numeric loops                                         */
/* ---------------------------------------------------------------- */

static int is_control_reg (int r, int for_a) {
  if (for_a < 0)
    return 0;   /* while loops have no control triplet */
  return r == for_a || r == for_a + 1 || r == for_a + 2;
}


/* Validate and compile the loop [start, backedge].  The trace must end in
** FORLOOP and contain only integer register arithmetic.  Returns 1 when a
** loop was compiled (or a previous one is already installed). */
static int slot_type (TValue *b, uint8_t *cur, uint8_t *def, int r) {
  if (def[r])
    return cur[r];
  if (ttisinteger(&b[r]))
    return LUAR_T_INT;
  if (ttisfloat(&b[r]))
    return LUAR_T_FLOAT;
  if (ttistable(&b[r]))
    return LUAR_T_TABLE;
  return 0;
}


static void add_guard (LuaR_LoopDesc *d, uint8_t *seen, int r, int type) {
  if (!seen[r] && d->nguards < 256) {
    seen[r] = 1;
    d->guard_reg[d->nguards] = (uint8_t)r;
    d->guard_type[d->nguards] = (uint8_t)type;
    d->nguards++;
  }
}


static int try_compile (lua_State *L, Proto *p, ptrdiff_t start,
                        ptrdiff_t backedge, void *base) {
  LuaR_JIT *J = G(L)->luar_jit;
  LuaR_Op ops[LUAR_JIT_MAXINS];
  LuaR_LoopDesc d;
  LuaR_LoopFn fn;
  size_t size = 0;
  ptrdiff_t n = backedge - start + 1;
  ptrdiff_t i;
  Instruction last;
  TValue *b = (TValue *)base;
  uint8_t cur[256], def[256], seen[256], paired[256];
  int for_a, has_branch = 0, is_while = 0, partial = 0;
  int call_at = -1;
  if (!luari_emit_available() || b == NULL || n < 2 || n > LUAR_JIT_MAXINS)
    return 0;
  /* The emitter hard-codes the default 16-byte TValue, 64-bit integers and
  ** double floats; other configurations stay on the interpreter. */
  if (sizeof(TValue) != 16 || offsetof(TValue, tt_) != 8 ||
      sizeof(lua_Integer) != 8 || sizeof(lua_Number) != sizeof(double))
    return 0;
  last = p->code[backedge];
  if (GET_OPCODE(last) == OP_FORLOOP) {
    for_a = GETARG_A(last);
    if (for_a < 0 || for_a + 2 > 255)
      return 0;
    if (!ttisinteger(&b[for_a]) || !ttisinteger(&b[for_a + 1]) ||
        !ttisinteger(&b[for_a + 2]))
      return 0;   /* float/non-integer loop control: not supported */
  }
  else if (GET_OPCODE(last) == OP_JMP && GETARG_sJ(last) < 0) {
    if (backedge <= 0)
      return 0;
    is_while = 1;
    for_a = -1;
  }
  else
    return 0;

  for (i = 0; i < n - 1; i++) {
    if (GET_OPCODE(p->code[start + i]) == OP_CALL) {
      call_at = (int)i;
      break;
    }
  }
  memset(&d, 0, sizeof(d));
  memset(cur, 0, sizeof(cur));
  memset(def, 0, sizeof(def));
  memset(seen, 0, sizeof(seen));
  memset(paired, 0, sizeof(paired));
  d.ops = ops;
  d.nops = (int)n - 1;
  d.for_a = for_a;
  d.is_while = is_while;
  d.tag_int = LUA_VNUMINT;
  d.tag_flt = LUA_VNUMFLT;
  d.tag_table = LUA_VTABLE;
  d.table_array_off = (int)offsetof(Table, array);
  d.table_asize_off = (int)offsetof(Table, asize);
  d.table_metatable_off = (int)offsetof(Table, metatable);
  if (!is_while) {
    for (i = 0; i < 3; i++) {
      cur[for_a + i] = LUAR_T_INT;
      def[for_a + i] = 1;
      add_guard(&d, seen, for_a + i, LUAR_T_INT);
    }
  }

  for (i = 0; i < n - 1; i++) {
    Instruction ins = p->code[start + i];
    LuaR_Op *op = &ops[i];
    int tb, tc;
    OpCode opc = GET_OPCODE(ins);
    memset(op, 0, sizeof(*op));
    if (opc == OP_EQ || opc == OP_LT || opc == OP_LE ||
        opc == OP_EQK || opc == OP_EQI || opc == OP_LTI ||
        opc == OP_LEI || opc == OP_GTI || opc == OP_GEI) {
      Instruction jins;
      ptrdiff_t j = i + 1, tgt;
      int aa = GETARG_A(ins), bb = -1, kk = GETARG_k(ins);
      int64_t imm = 0;
      double cnum = 0.0;
      int at, bt = LUAR_T_INT;
      LuaR_OpKind ck;
      if (j >= n - 1 &&
          !(is_while && j == n - 1 &&
            GET_OPCODE(p->code[start + j]) == OP_JMP)) {
        return 0;
      }
      jins = p->code[start + j];
      if (GET_OPCODE(jins) != OP_JMP) return 0;
      switch (opc) {
        case OP_EQ: ck = LUAR_OP_EQ; bb = GETARG_B(ins); break;
        case OP_LT: ck = LUAR_OP_LT; bb = GETARG_B(ins); break;
        case OP_LE: ck = LUAR_OP_LE; bb = GETARG_B(ins); break;
        case OP_EQK: {
          int idx = GETARG_B(ins);
          TValue *kkv;
          if (idx < 0 || idx >= p->sizek) return 0;
          kkv = &p->k[idx];
          ck = LUAR_OP_EQ;
          if (ttisinteger(kkv)) {
            imm = (int64_t)ivalue(kkv);
            bt = LUAR_T_INT;
          }
          else if (ttisfloat(kkv)) {
            cnum = (double)fltvalue(kkv);
            bt = LUAR_T_FLOAT;
          }
          else
            return 0;
          break;
        }
        case OP_EQI: ck = LUAR_OP_EQ; imm = GETARG_sB(ins); break;
        case OP_LTI: ck = LUAR_OP_LT; imm = GETARG_sB(ins); break;
        case OP_LEI: ck = LUAR_OP_LE; imm = GETARG_sB(ins); break;
        case OP_GTI: ck = LUAR_OP_GT; imm = GETARG_sB(ins); break;
        case OP_GEI: ck = LUAR_OP_GE; imm = GETARG_sB(ins); break;
        default: return 0;
      }
      at = slot_type(b, cur, def, aa);
      if (at == 0 || at == LUAR_T_TABLE) return 0;
      if (bb >= 0) {
        bt = slot_type(b, cur, def, bb);
        if (bt == 0 || bt == LUAR_T_TABLE) return 0;
      }
      tgt = (start + j) + 1 + GETARG_sJ(jins);
      if (tgt < start || tgt == start + j ||
          (tgt <= start + i && tgt != start))
        return 0;
      if (tgt > backedge) {
        if (tgt != backedge + 1) return 0;
        tgt = is_while ? backedge : backedge + 1;   /* break/exit */
      }
      op->kind = ck;
      op->a = (uint8_t)aa;
      op->b = (uint8_t)bb;
      op->ta = (uint8_t)at;
      op->tb = (uint8_t)bt;
      op->imm = imm;
      op->num = cnum;
      op->k = (uint8_t)(kk != 0);
      op->target = (int32_t)(tgt - start);
      memset(&ops[j], 0, sizeof(ops[j]));
      ops[j].kind = LUAR_OP_JMP;
      ops[j].target = (int32_t)(tgt - start);
      paired[j] = 1;
      has_branch = 1;
      i = j;
      continue;
    }
    if (opc == OP_JMP) {
      ptrdiff_t tgt = (start + i) + 1 + GETARG_sJ(ins);
      if (tgt < start || (tgt <= start + i && tgt != start)) return 0;
      if (tgt > backedge) {
        if (tgt != backedge + 1) return 0;
        tgt = is_while ? backedge : backedge + 1;   /* break */
      }
      op->kind = LUAR_OP_JMP;
      op->target = (int32_t)(tgt - start);
      has_branch = 1;
      continue;
    }
    switch (opc) {
      case OP_MOVE:
        if (is_control_reg(GETARG_A(ins), for_a)) return 0;
        tb = slot_type(b, cur, def, GETARG_B(ins));
        if (tb == 0 || tb == LUAR_T_TABLE ||
            (tb != LUAR_T_INT && tb != LUAR_T_FLOAT)) {
          if (J->enable_partial && call_at > (int)i && i > 0 &&
              !has_branch) {
            d.partial = 1;
            d.exit_pc = (int)(start + i);
            d.nops = (int)i;
            partial = 1;
            goto decode_done;
          }
          return 0;
        }
        add_guard(&d, seen, GETARG_B(ins), tb);
        op->kind = LUAR_OP_MOVE;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        op->tb = (uint8_t)tb;
        cur[op->a] = (uint8_t)tb;
        def[op->a] = 1;
        break;
      case OP_LOADI:
        if (is_control_reg(GETARG_A(ins), for_a)) return 0;
        op->kind = LUAR_OP_LOADI;
        op->a = (uint8_t)GETARG_A(ins);
        op->imm = (int64_t)GETARG_sBx(ins);
        op->ta = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      case OP_LOADF:
        if (is_control_reg(GETARG_A(ins), for_a)) return 0;
        op->kind = LUAR_OP_LOADF;
        op->a = (uint8_t)GETARG_A(ins);
        op->num = (double)GETARG_sBx(ins);
        op->ta = LUAR_T_FLOAT;
        cur[op->a] = LUAR_T_FLOAT;
        def[op->a] = 1;
        break;
      case OP_LOADK: {
        int idx = (int)GETARG_Bx(ins);
        TValue *k;
        if (idx < 0 || idx >= p->sizek) return 0;
        if (is_control_reg(GETARG_A(ins), for_a)) return 0;
        k = &p->k[idx];
        op->kind = LUAR_OP_LOADK;
        op->a = (uint8_t)GETARG_A(ins);
        if (ttisinteger(k)) {
          op->ta = LUAR_T_INT;
          op->imm = (int64_t)ivalue(k);
        }
        else if (ttisfloat(k)) {
          op->ta = LUAR_T_FLOAT;
          op->num = (double)fltvalue(k);
        }
        else
          return 0;
        cur[op->a] = op->ta;
        def[op->a] = 1;
        break;
      }
      case OP_ADDI:
        if (is_control_reg(GETARG_A(ins), for_a)) return 0;
        if (slot_type(b, cur, def, GETARG_B(ins)) != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        op->kind = LUAR_OP_ADDI;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        op->imm = (int64_t)GETARG_sC(ins);
        op->ta = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      case OP_ADD:
      case OP_SUB:
      case OP_MUL:
      case OP_DIV: {
        int ka = GETARG_A(ins);
        if (is_control_reg(ka, for_a)) return 0;
        tb = slot_type(b, cur, def, GETARG_B(ins));
        tc = slot_type(b, cur, def, GETARG_C(ins));
        if (tb == 0 || tc == 0 ||
            tb == LUAR_T_TABLE || tc == LUAR_T_TABLE) return 0;
        add_guard(&d, seen, GETARG_B(ins), tb);
        add_guard(&d, seen, GETARG_C(ins), tc);
        op->kind = GET_OPCODE(ins) == OP_ADD ? LUAR_OP_ADD :
                   GET_OPCODE(ins) == OP_SUB ? LUAR_OP_SUB :
                   GET_OPCODE(ins) == OP_MUL ? LUAR_OP_MUL : LUAR_OP_DIV;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->c = (uint8_t)GETARG_C(ins);
        op->tb = (uint8_t)tb;
        op->tc = (uint8_t)tc;
        op->ta = (op->kind == LUAR_OP_DIV || tb == LUAR_T_FLOAT ||
                  tc == LUAR_T_FLOAT) ? LUAR_T_FLOAT : LUAR_T_INT;
        cur[op->a] = op->ta;
        def[op->a] = 1;
        break;
      }
      case OP_ADDK:
      case OP_SUBK:
      case OP_MULK:
      case OP_DIVK: {
        int ka = GETARG_A(ins);
        int idx = (int)GETARG_C(ins);
        TValue *k;
        int ct;
        if (is_control_reg(ka, for_a)) return 0;
        if (idx < 0 || idx >= p->sizek) return 0;
        k = &p->k[idx];
        ct = ttisinteger(k) ? LUAR_T_INT :
             ttisfloat(k) ? LUAR_T_FLOAT : 0;
        if (ct == 0) return 0;
        tb = slot_type(b, cur, def, GETARG_B(ins));
        if (tb == 0 || tb == LUAR_T_TABLE) return 0;
        add_guard(&d, seen, GETARG_B(ins), tb);
        op->kind = GET_OPCODE(ins) == OP_ADDK ? LUAR_OP_ADDK :
                   GET_OPCODE(ins) == OP_SUBK ? LUAR_OP_SUBK :
                   GET_OPCODE(ins) == OP_MULK ? LUAR_OP_MULK : LUAR_OP_DIVK;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->tb = (uint8_t)tb;
        op->ta = (op->kind == LUAR_OP_DIVK || tb == LUAR_T_FLOAT ||
                  ct == LUAR_T_FLOAT) ? LUAR_T_FLOAT : LUAR_T_INT;
        if (ct == LUAR_T_INT) {
          op->imm = (int64_t)ivalue(k);
          op->num = (double)op->imm;   /* for a float-typed result */
        }
        else {
          op->num = (double)fltvalue(k);
        }
        cur[op->a] = op->ta;
        def[op->a] = 1;
        break;
      }
      case OP_GETTABLE: {
        int ka = GETARG_A(ins), kb = GETARG_B(ins), kc = GETARG_C(ins);
        if (i != 0 || d.has_table) return 0;   /* only a leading lookup */
        if (is_control_reg(ka, for_a)) return 0;
        if (slot_type(b, cur, def, kb) != LUAR_T_TABLE) return 0;
        if (slot_type(b, cur, def, kc) != LUAR_T_INT) return 0;
        op->kind = LUAR_OP_GETTABLE;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)kb;
        op->c = (uint8_t)kc;
        op->ta = LUAR_T_INT;   /* the native path accepts integer elements */
        op->tb = LUAR_T_TABLE;
        op->tc = LUAR_T_INT;
        cur[ka] = LUAR_T_INT;
        def[ka] = 1;
        d.has_table = 1;
        d.table_reg = kb;
        has_branch = 1;   /* use the labelled emitter and typed guards */
        /* Fuse the common "acc = acc + t[key]" shape. */
        if (i + 1 < n - 1) {
          Instruction nx = p->code[start + i + 1];
          if (GET_OPCODE(nx) == OP_ADD) {
            int na = GETARG_A(nx), nb = GETARG_B(nx), nc = GETARG_C(nx);
            int acct = 0;
            if ((nb == ka && nc == na) || (nc == ka && nb == na))
              acct = slot_type(b, cur, def, na);
            if ((acct == LUAR_T_INT || acct == LUAR_T_FLOAT) &&
                !is_control_reg(na, for_a)) {
              op->kind = LUAR_OP_TABLE_ADD;
              op->a = (uint8_t)na;
              op->b = (uint8_t)kb;
              op->c = (uint8_t)kc;
              op->ta = (uint8_t)acct;
              op->tb = LUAR_T_TABLE;
              op->tc = LUAR_T_INT;
              memset(&ops[i + 1], 0, sizeof(ops[i + 1]));
              ops[i + 1].kind = LUAR_OP_NOP;
              cur[na] = (uint8_t)acct;
              def[na] = 1;
              i++;
            }
          }
        }
        break;
      }
      case OP_SETTABLE: {
        int ka = GETARG_A(ins), kb = GETARG_B(ins), kc = GETARG_C(ins);
        int vt;
        if (i != 0 || d.has_table) return 0;
        if (slot_type(b, cur, def, ka) != LUAR_T_TABLE) return 0;
        if (slot_type(b, cur, def, kb) != LUAR_T_INT) return 0;
        vt = slot_type(b, cur, def, kc);
        if (vt != LUAR_T_INT && vt != LUAR_T_FLOAT) return 0;
        op->kind = LUAR_OP_SETTABLE;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)kb;
        op->c = (uint8_t)kc;
        op->ta = (uint8_t)vt;
        op->tb = LUAR_T_TABLE;
        op->tc = LUAR_T_INT;
        d.has_table = 1;
        d.table_reg = ka;
        has_branch = 1;
        break;
      }
      case OP_MMBIN:
      case OP_MMBINI:
      case OP_MMBINK:
        op->kind = LUAR_OP_MMBIN;
        break;
      case OP_CLOSE:
        /* No closure/TBC opcode is whitelisted in the body, so an
        ** upvalue close emitted for 'break' has nothing to close. */
        op->kind = LUAR_OP_NOP;
        break;
      case OP_CALL:
        /* A call-free numeric prefix can run natively and hand the CALL
        ** itself back to the interpreter.  Branches or an empty prefix
        ** are not supported. */
        if (call_at == (int)i && i > 0 && !has_branch && !d.has_table &&
            J->enable_partial) {
          d.partial = 1;
          d.exit_pc = (int)(start + i);
          d.nops = (int)i;
          partial = 1;
          goto decode_done;
        }
        return 0;
      default:
        if (J->enable_partial && call_at > (int)i && i > 0 &&
            !has_branch) {
          d.partial = 1;
          d.exit_pc = (int)(start + i);
          d.nops = (int)i;
          partial = 1;
          goto decode_done;
        }
        return 0;
    }
    if (op->kind != LUAR_OP_MMBIN && is_control_reg(op->a, for_a))
      return 0;
    /* A loop-carried register whose result flavour differs from the
    ** operand flavour would change type between native iterations, while
    ** the emitted code is specialised for one flavour.  Fall back. */
    if (op->kind == LUAR_OP_ADD || op->kind == LUAR_OP_SUB ||
        op->kind == LUAR_OP_MUL || op->kind == LUAR_OP_DIV) {
      if (op->a == op->b && op->ta != op->tb) return 0;
      if (op->a == op->c && op->ta != op->tc) return 0;
    }
    else if (op->kind == LUAR_OP_ADDK || op->kind == LUAR_OP_SUBK ||
             op->kind == LUAR_OP_MULK || op->kind == LUAR_OP_DIVK) {
      if (op->a == op->b && op->ta != op->tb) return 0;
    }
  }
decode_done:
  if (partial) {
    d.npc = (int)n;
    d.has_branch = 0;
    goto emit_ready;
  }
  /* Fast subtraction: acc = acc - idx. */
  if (!has_branch && !is_while && n == 3) {
    LuaR_Op *sub = &ops[0];
    int only = 1;
    for (i = 1; i < n - 1; i++)
      if (ops[i].kind != LUAR_OP_MMBIN) only = 0;
    if (only && sub->kind == LUAR_OP_SUB && sub->a == sub->b &&
        sub->c == for_a + 2 &&
        (sub->ta == LUAR_T_INT || sub->ta == LUAR_T_FLOAT) &&
        !is_control_reg(sub->a, for_a)) {
      memset(seen, 0, sizeof(seen));
      d.nguards = 0;
      add_guard(&d, seen, sub->a, sub->ta);
      add_guard(&d, seen, for_a, LUAR_T_INT);
      add_guard(&d, seen, for_a + 1, LUAR_T_INT);
      add_guard(&d, seen, for_a + 2, LUAR_T_INT);
      d.fast_sub = 1;
      d.fsb_float = (sub->ta == LUAR_T_FLOAT);
      d.fsb_acc = sub->a;
    }
  }

  /* Fast product: acc = acc * idx. */
  if (!has_branch && !is_while && n == 3) {
    LuaR_Op *mul = &ops[0];
    if (((mul->kind == LUAR_OP_MUL &&
          (mul->ta == LUAR_T_INT || mul->ta == LUAR_T_FLOAT)) ||
         (mul->kind == LUAR_OP_DIV && mul->ta == LUAR_T_FLOAT)) &&
        mul->a == mul->b && mul->c == for_a + 2 &&
        !is_control_reg(mul->a, for_a)) {
      memset(seen, 0, sizeof(seen));
      d.nguards = 0;
      add_guard(&d, seen, mul->a, mul->ta);
      add_guard(&d, seen, for_a, LUAR_T_INT);
      add_guard(&d, seen, for_a + 1, LUAR_T_INT);
      add_guard(&d, seen, for_a + 2, LUAR_T_INT);
      d.fast_prod = 1;
      d.fp_float = (mul->ta == LUAR_T_FLOAT);
      d.fp_div = (mul->kind == LUAR_OP_DIV);
      d.fp_acc = mul->a;
    }
  }

  /* Fast square sum: acc = acc + idx * idx. */
  if (!has_branch && !is_while && n == 5) {
    LuaR_Op *mul = &ops[0];
    LuaR_Op *add = &ops[2];
    int only = 1;
    for (i = 1; i < n - 1; i++)
      if (i != 2 && ops[i].kind != LUAR_OP_MMBIN) only = 0;
    if (only && mul->kind == LUAR_OP_MUL && mul->a != for_a + 2 &&
        mul->b == for_a + 2 && mul->c == for_a + 2 &&
        mul->ta == LUAR_T_INT && add->kind == LUAR_OP_ADD &&
        add->a == add->b && add->c == mul->a &&
        (add->ta == LUAR_T_INT || add->ta == LUAR_T_FLOAT) &&
        !is_control_reg(add->a, for_a)) {
      memset(seen, 0, sizeof(seen));
      d.nguards = 0;
      add_guard(&d, seen, add->a, add->ta);
      add_guard(&d, seen, for_a, LUAR_T_INT);
      add_guard(&d, seen, for_a + 1, LUAR_T_INT);
      add_guard(&d, seen, for_a + 2, LUAR_T_INT);
      d.fast_sq = 1;
      d.fs_float = (add->ta == LUAR_T_FLOAT);
      d.fs_acc = add->a;
    }
  }

  /* Fast while sum: while idx < limit do acc = acc + idx; idx = idx + 1. */
  if (is_while && n >= 5) {

    int acc = -1, idx = -1, limit = -1, kind = 0, ok = 1, has_imm = 0;
    int inc_i = -1, temp = -1, square = 0, prod = 0;
    int is_float = 0, accum_i = -1, sub = 0, div = 0, linear = 0;
    double fconst = 0.0;
    int64_t imm = 0;
    for (i = 0; i < n - 1 && ok; i++) {
      LuaR_Op *op = &ops[i];
      if (op->kind == LUAR_OP_MMBIN || op->kind == LUAR_OP_NOP ||
          op->kind == LUAR_OP_JMP)
        continue;
      if (i == 0) {
        if (op->kind != LUAR_OP_LT && op->kind != LUAR_OP_LE) { ok = 0; break; }
        kind = op->kind;
        idx = op->a;
        if (op->b == 0xFF) { imm = op->imm; has_imm = 1; }
        else limit = op->b;
      }
      else if (op->kind == LUAR_OP_MUL) {
        if (op->a == op->b && op->c == idx && op->a != idx &&
            (op->ta == LUAR_T_INT || op->ta == LUAR_T_FLOAT)) {
          prod = 1;
          is_float = (op->ta == LUAR_T_FLOAT);
          acc = op->a;
          accum_i = (int)i;
        }
        else if (temp < 0 && op->b == idx && op->c == idx &&
                 op->a != idx && op->ta == LUAR_T_INT) {
          temp = op->a;
        }
        else { ok = 0; break; }
      }
      else if (op->kind == LUAR_OP_MULK) {
        if (temp >= 0 || op->b != idx || op->ta != LUAR_T_FLOAT ||
            op->a == idx) { ok = 0; break; }
        temp = op->a;
        is_float = 1;
        linear = 1;
        fconst = op->num;
      }
      else if (op->kind == LUAR_OP_DIV) {
        if (prod || acc >= 0 || op->a != op->b || op->c != idx ||
            op->ta != LUAR_T_FLOAT) { ok = 0; break; }
        acc = op->a;
        is_float = 1;
        div = 1;
        accum_i = (int)i;
      }
      else if (op->kind == LUAR_OP_ADD) {
        if (prod || acc >= 0 || op->a != op->b) { ok = 0; break; }
        if (is_float) {
          if (op->ta != LUAR_T_FLOAT || op->c != temp) { ok = 0; break; }
          square = 0;
        }
        else if (op->c == idx && temp < 0) {
          if (op->ta != LUAR_T_INT && op->ta != LUAR_T_FLOAT) {
            ok = 0; break;
          }
          square = 0;
          is_float = (op->ta == LUAR_T_FLOAT);
        }
        else if (op->c == temp && temp >= 0) {
          if (op->ta != LUAR_T_INT && op->ta != LUAR_T_FLOAT) {
            ok = 0; break;
          }
          square = 1;
          is_float = (op->ta == LUAR_T_FLOAT);
        }
        else { ok = 0; break; }
        acc = op->a;
        accum_i = (int)i;
      }
      else if (op->kind == LUAR_OP_SUB) {
        if (prod || acc >= 0 || op->a != op->b || op->c != idx ||
            (op->ta != LUAR_T_INT && op->ta != LUAR_T_FLOAT)) {
          ok = 0; break;
        }
        acc = op->a;
        is_float = (op->ta == LUAR_T_FLOAT);
        sub = 1;
        accum_i = (int)i;
      }
      else if (op->kind == LUAR_OP_ADDI) {
        if (op->a != idx || op->b != idx || op->imm == 0 ||
            op->ta != LUAR_T_INT) { ok = 0; break; }
        inc_i = (int)i;
      }
      else ok = 0;
    }
    if (ok && acc >= 0 && idx >= 0 && accum_i >= 0 && inc_i > accum_i &&
        (limit >= 0 || has_imm)) {
      memset(seen, 0, sizeof(seen));
      d.nguards = 0;
      add_guard(&d, seen, acc, is_float ? LUAR_T_FLOAT : LUAR_T_INT);
      add_guard(&d, seen, idx, LUAR_T_INT);
      if (limit >= 0)
        add_guard(&d, seen, limit, LUAR_T_INT);
      d.fast_while = 1;
      d.fw_square = square;
      d.fw_prod = prod;
      d.fw_sub = sub;
      d.fw_div = div;
      d.fw_linear = linear;
      d.fw_float = is_float;
      d.fw_const = fconst;
      d.fw_acc = acc;
      d.fw_idx = idx;
      d.fw_limit = limit >= 0 ? limit : 0xFF;
      d.fw_kind = kind;
      d.fw_k = ops[0].k;
      d.fw_imm = imm;
      d.fw_step = ops[inc_i].imm;
    }
  }

  /* Fast table sum: acc = acc + t[idx]. */
  if (!is_while && d.nops >= 1 && ops[0].kind == LUAR_OP_TABLE_ADD &&
      ops[0].c == for_a + 2) {
    int only = 1;
    for (i = 1; i < n - 1; i++)
      if (ops[i].kind != LUAR_OP_MMBIN && ops[i].kind != LUAR_OP_NOP)
        only = 0;
    if (only) {
      memset(seen, 0, sizeof(seen));
      d.nguards = 0;
      add_guard(&d, seen, ops[0].tb, LUAR_T_TABLE);
      add_guard(&d, seen, ops[0].c, LUAR_T_INT);
      add_guard(&d, seen, ops[0].a, ops[0].ta);
      add_guard(&d, seen, for_a, LUAR_T_INT);
      add_guard(&d, seen, for_a + 1, LUAR_T_INT);
      add_guard(&d, seen, for_a + 2, LUAR_T_INT);
      d.fast_tsum = 1;
      d.ft_table = ops[0].b;
      d.ft_idx = ops[0].c;
      d.ft_acc = ops[0].a;
    }
  }

  /* Fast fill: t[idx] = idx with the loop index as both key and value. */
  if (!is_while && d.nops == 1 && ops[0].kind == LUAR_OP_SETTABLE &&
      ops[0].b == for_a + 2 && ops[0].c == for_a + 2) {
    d.fast_fill = 1;
    d.ff_table = ops[0].a;
  }
  if (d.has_table && !J->enable_table)
    return 0;

  /* Fast integer sum: acc = acc + idx. */
  if (!has_branch && !is_while && n == 3) {
    LuaR_Op *add = &ops[0];
    int only = 1;
    for (i = 1; i < n - 1; i++)
      if (ops[i].kind != LUAR_OP_MMBIN) only = 0;
    if (only && add->kind == LUAR_OP_ADD && add->a == add->b &&
        add->c == for_a + 2 &&
        (add->ta == LUAR_T_INT || add->ta == LUAR_T_FLOAT) &&
        !is_control_reg(add->a, for_a)) {
      memset(seen, 0, sizeof(seen));
      d.nguards = 0;
      add_guard(&d, seen, add->a, add->ta);
      add_guard(&d, seen, for_a, LUAR_T_INT);
      add_guard(&d, seen, for_a + 1, LUAR_T_INT);
      add_guard(&d, seen, for_a + 2, LUAR_T_INT);
      d.fast_int = 1;
      d.fi_float = (add->ta == LUAR_T_FLOAT);
      d.fi_acc = add->a;
    }
  }

  /* Fast float pattern: acc = acc + idx * constant (the float_loop
  ** benchmark shape).  Keep acc in xmm0 and the loop counters in GPRs. */
  if (!has_branch && !is_while && n >= 3) {
    int temp = -1, acc = -1, idx = -1;
    double konst = 0.0;
    int is_float = 0, seen_mul = 0, seen_add = 0, ok = 1;
    int64_t iconst = 0;
    for (i = 0; i < n - 1 && ok; i++) {
      LuaR_Op *op = &ops[i];
      switch (op->kind) {
        case LUAR_OP_MMBIN:
          break;
        case LUAR_OP_MULK:
          if (!seen_mul && (op->ta == LUAR_T_FLOAT ||
                            op->ta == LUAR_T_INT)) {
            temp = op->a;
            idx = op->b;
            konst = op->num;
            iconst = op->imm;
            is_float = (op->ta == LUAR_T_FLOAT);
            seen_mul = 1;
          }
          else ok = 0;
          break;
        case LUAR_OP_ADD:
          if (!seen_add && op->a == op->b && op->c == temp &&
              op->ta == (is_float ? LUAR_T_FLOAT : LUAR_T_INT)) {
            acc = op->a;
            seen_add = 1;
          }
          else ok = 0;
          break;
        default:
          ok = 0;
          break;
      }
    }
    if (ok && seen_mul && seen_add && idx == for_a + 2 && acc >= 0 &&
        !is_control_reg(acc, for_a) && temp != acc) {
      memset(seen, 0, sizeof(seen));
      d.nguards = 0;
      add_guard(&d, seen, acc, is_float ? LUAR_T_FLOAT : LUAR_T_INT);
      add_guard(&d, seen, idx, LUAR_T_INT);
      add_guard(&d, seen, for_a, LUAR_T_INT);
      add_guard(&d, seen, for_a + 1, LUAR_T_INT);
      d.fast_float = 1;
      d.ff_acc = acc;
      d.ff_idx = idx;
      d.ff_const = konst;
      d.ff_is_float = is_float;
      d.ff_iconst = iconst;
    }
  }

  if (has_branch) {
    /* Numeric branch loops: guard a read only when no earlier write is
    ** guaranteed to dominate it.  A write in a skipped branch body does
    ** not dominate a later read, so a branch target between the write and
    ** the read forces a guard. */
    uint8_t targets[256];
    int last_write[256];
    memset(seen, 0, sizeof(seen));
    memset(targets, 0, sizeof(targets));
    for (i = 0; i < n - 1; i++) {
      LuaR_Op *op = &ops[i];
      if (op->kind == LUAR_OP_JMP || op->kind == LUAR_OP_LT ||
          op->kind == LUAR_OP_LE || op->kind == LUAR_OP_GT ||
          op->kind == LUAR_OP_GE || op->kind == LUAR_OP_EQ) {
        if (op->target >= 0 && op->target < 256)
          targets[op->target] = 1;
      }
    }
    for (i = 0; i < 256; i++)
      last_write[i] = -1;
    d.nguards = 0;
    for (i = 0; i < n - 1; i++) {
      LuaR_Op *op = &ops[i];
      int reads[4], types[4], nr = 0, q;
      switch (op->kind) {
        case LUAR_OP_MOVE:
          reads[nr] = op->b; types[nr++] = op->tb; break;
        case LUAR_OP_ADD:
        case LUAR_OP_SUB:
        case LUAR_OP_MUL:
        case LUAR_OP_DIV:
          reads[nr] = op->b; types[nr++] = op->tb;
          reads[nr] = op->c; types[nr++] = op->tc;
          break;
        case LUAR_OP_ADDI:
          reads[nr] = op->b; types[nr++] = LUAR_T_INT; break;
        case LUAR_OP_GETTABLE:
          reads[nr] = op->b; types[nr++] = LUAR_T_TABLE;
          reads[nr] = op->c; types[nr++] = LUAR_T_INT;
          break;
        case LUAR_OP_TABLE_ADD:
          reads[nr] = op->b; types[nr++] = LUAR_T_TABLE;
          reads[nr] = op->c; types[nr++] = LUAR_T_INT;
          reads[nr] = op->a; types[nr++] = op->ta;
          break;
        case LUAR_OP_SETTABLE:
          reads[nr] = op->a; types[nr++] = LUAR_T_TABLE;
          reads[nr] = op->b; types[nr++] = LUAR_T_INT;
          reads[nr] = op->c; types[nr++] = op->ta;
          break;
        case LUAR_OP_ADDK:
        case LUAR_OP_SUBK:
        case LUAR_OP_MULK:
        case LUAR_OP_DIVK:
          reads[nr] = op->b; types[nr++] = op->tb; break;
        case LUAR_OP_LT:
        case LUAR_OP_LE:
        case LUAR_OP_GT:
        case LUAR_OP_GE:
        case LUAR_OP_EQ:
          reads[nr] = op->a; types[nr++] = op->ta;
          if (op->b != 0xFF) {
            reads[nr] = op->b; types[nr++] = op->tb;
          }
          break;
        default: break;
      }
      for (q = 0; q < nr; q++) {
        int r = reads[q], j = last_write[r], t;
        int safe = (j >= 0);
        for (t = j + 1; safe && t <= (int)i; t++) {
          if (targets[t]) safe = 0;
        }
        if (!safe)
          add_guard(&d, seen, r, types[q]);
      }
      switch (op->kind) {
        case LUAR_OP_MOVE:
        case LUAR_OP_LOADI:
        case LUAR_OP_LOADF:
        case LUAR_OP_LOADK:
        case LUAR_OP_ADDI:
        case LUAR_OP_ADD:
        case LUAR_OP_SUB:
        case LUAR_OP_MUL:
        case LUAR_OP_DIV:
        case LUAR_OP_ADDK:
        case LUAR_OP_SUBK:
        case LUAR_OP_MULK:
        case LUAR_OP_DIVK:
        case LUAR_OP_GETTABLE:
        case LUAR_OP_TABLE_ADD:
          last_write[op->a] = (int)i;
          break;
        default: break;
      }
      if (op->kind == LUAR_OP_JMP || op->kind == LUAR_OP_LT ||
          op->kind == LUAR_OP_LE || op->kind == LUAR_OP_GT ||
          op->kind == LUAR_OP_GE || op->kind == LUAR_OP_EQ) {
        if (op->target < 0 || op->target > n ||
            (op->target == n && is_while) ||
            (op->target < n && paired[op->target])) {
          return 0;
        }
      }
    }
    if (!is_while) {
      for (i = 0; i < 3; i++)
        add_guard(&d, seen, for_a + i, LUAR_T_INT);
    }
    d.npc = (int)n;
    d.has_branch = 1;
  }
  if (is_while && d.nops >= 1 && paired[n - 1] &&
      (ops[d.nops - 1].kind == LUAR_OP_LT ||
       ops[d.nops - 1].kind == LUAR_OP_LE ||
       ops[d.nops - 1].kind == LUAR_OP_GT ||
       ops[d.nops - 1].kind == LUAR_OP_GE ||
       ops[d.nops - 1].kind == LUAR_OP_EQ))
    d.repeat = 1;
  if (d.repeat && d.nops >= 3) {
    LuaR_Op *cmp = &ops[d.nops - 1];
    LuaR_Op *addi = NULL, *accop = NULL;
    int idx = -1, acc = -1, limit = -1, imm = 0, left, opkind = 0;
    int addi_i = -1, acc_i = -1, temp = -1, temp_i = -1;
    for (i = 0; i < d.nops - 1; i++) {
      LuaR_Op *op = &ops[i];
      if (op->kind == LUAR_OP_ADDI && addi == NULL &&
          op->a == op->b && op->ta == LUAR_T_INT) {
        addi = op;
        idx = op->a;
        addi_i = (int)i;
      }
      else if (addi != NULL && temp < 0 && op->kind == LUAR_OP_MUL &&
               op->b == idx && op->c == idx && op->a != idx &&
               op->ta == LUAR_T_INT) {
        temp = op->a;
        temp_i = (int)i;
      }
      else if (addi != NULL && accop == NULL && temp >= 0 &&
               op->kind == LUAR_OP_ADD && op->a == op->b &&
               op->c == temp &&
               (op->ta == LUAR_T_INT || op->ta == LUAR_T_FLOAT)) {
        accop = op;
        acc_i = (int)i;
        opkind = 3;
      }
      else if (addi != NULL && accop == NULL && op->a == op->b &&
               op->c == idx &&
               (op->ta == LUAR_T_INT || op->ta == LUAR_T_FLOAT) &&
               (op->kind == LUAR_OP_ADD || op->kind == LUAR_OP_SUB ||
                op->kind == LUAR_OP_MUL)) {
        accop = op;
        acc_i = (int)i;
        opkind = (op->kind == LUAR_OP_SUB) ? 1 :
                 (op->kind == LUAR_OP_MUL) ? 2 : 0;
      }
    }
    if (addi != NULL && accop != NULL && acc_i > addi_i &&
        (opkind != 3 || temp_i > addi_i) && idx >= 0) {
      acc = accop->a;
      if (cmp->a == idx && cmp->b != idx) {
        left = 1;
        if (cmp->b == 0xFF) imm = 1;
        else limit = cmp->b;
      }
      else if (cmp->b == idx) {
        left = 0;
        if (cmp->a == 0xFF) imm = 1;
        else limit = cmp->a;
      }
      else {
        acc = -1;
        left = 0;
      }
      if (acc >= 0 && !is_control_reg(acc, -1)) {
        memset(seen, 0, sizeof(seen));
        d.nguards = 0;
        add_guard(&d, seen, acc, accop->ta);
        add_guard(&d, seen, idx, LUAR_T_INT);
        if (limit >= 0)
          add_guard(&d, seen, limit, LUAR_T_INT);
        d.fast_repeat = 1;
        d.fr_op = opkind;
        d.fr_float = (accop->ta == LUAR_T_FLOAT);
        d.fr_acc = acc;
        d.fr_idx = idx;
        d.fr_limit = limit >= 0 ? limit : 0xFF;
        d.fr_imm = imm;
        d.fr_immediate = imm ? cmp->imm : 0;
        d.fr_step = (int)addi->imm;
        d.fr_kind = cmp->kind;
        d.fr_k = cmp->k;
        d.fr_idx_left = left;
      }
    }
  }
emit_ready:
  fn = luari_emit_build(&d, &size);
  if (fn == NULL)
    return 0;
  if (J->ncode >= LUAR_JIT_MAXCODE) {
    munmap((void *)fn, size);
    return 0;
  }
  J->code[J->ncode].code = (void *)fn;
  J->code[J->ncode].size = size;
  J->ncode++;
  J->compiled++;
  J->mcode += size;
  {
    int slot = -1, k;
    for (k = 0; k < 2; k++) {
      if (p->luar_code[k] == NULL || p->luar_code_pc[k] == (int)backedge) {
        slot = k;
        break;
      }
    }
    if (slot < 0)
      slot = 1;   /* keep the first loop, replace the most recent */
    p->luar_code[slot] = (void *)fn;
    p->luar_code_pc[slot] = (int)backedge;
    p->luar_code_base[slot] = for_a;
    p->luar_code_epoch[slot] = J->epoch;
  }
  return 1;
}


int luari_jit_try_run (lua_State *L, Proto *p, int pc, void *base,
                       int expected_base) {
  LuaR_JIT *J = G(L)->luar_jit;
  int i;
  if (p == NULL)
    return 0;
  if (J == NULL || !J->enabled)
    return 0;
  if (L->hookmask != 0)
    return 0;
  for (i = 0; i < 2; i++) {
    if (p->luar_code[i] == NULL || p->luar_code_pc[i] != pc ||
        p->luar_code_base[i] != expected_base)
      continue;
    if (p->luar_code_epoch[i] != J->epoch) {
      p->luar_code[i] = NULL;   /* flushed: mapping was released */
      continue;
    }
    {
      int r = ((LuaR_LoopFn)p->luar_code[i])(base);
      if (r == 0)
        J->exits++;
      return r;
    }
  }
  return 0;
}


LUA_API int luar_has_jit (void) {
#if defined(LUAR_JIT_DISABLE)
  return 0;
#else
  return luari_emit_available();
#endif
}


/* ---------------------------------------------------------------- */
/* Recording                                                         */
/* ---------------------------------------------------------------- */

int luari_jit_hotloop (lua_State *L, Proto *p, const Instruction *backedge,
                       void *base) {
  LuaR_JIT *J = G(L)->luar_jit;
  Instruction ins;
  ptrdiff_t off, target, n;

  if (J == NULL) {
    luari_jit_init(L);
    J = G(L)->luar_jit;
    if (J == NULL)
      return 0;
  }
  J->hotloops++;
  if (!J->enabled)
    return 0;
  if (J->traces >= LUAR_JIT_MAXTRACES) {
    J->aborts++;
    return 0;
  }
  if (backedge < p->code || backedge >= p->code + p->sizecode)
    return 0;
  ins = *backedge;
  off = backedge - p->code;
  switch (GET_OPCODE(ins)) {
    case OP_JMP:
      target = off + 1 + GETARG_sJ(ins);
      break;
    case OP_FORLOOP:
    case OP_TFORLOOP:
      target = off + 1 - (ptrdiff_t)GETARG_Bx(ins);
      break;
    default:
      J->aborts++;
      return 0;
  }
  if (target < 0 || target > off) {
    J->aborts++;
    return 0;
  }
  n = off - target + 1;
  if (n <= 0 || n > LUAR_JIT_MAXINS) {
    J->aborts++;
    return 0;
  }
  {
    LuaR_Trace *t = &J->trace[J->traces];
    t->startpc = (uint32_t)target + 1;
    t->endpc = (uint32_t)off + 1;
    t->nins = (uint32_t)n;
    memcpy(t->code, p->code + target, (size_t)n * sizeof(Instruction));
    J->traces++;
    if (J->dump_trace || J->dump_ir) {
      int top = lua_gettop(L);
      if (J->dump_trace) {
        pushtrace(L, t);
        fprintf(stderr, "-- trace %u (pc %u..%u)\n%s",
                (unsigned)J->traces, (unsigned)t->startpc,
                (unsigned)t->endpc,
                lua_tostring(L, -1));
        lua_pop(L, 1);
      }
      if (J->dump_ir) {
        pushir(L, t);
        fprintf(stderr, "-- ir %u\n%s", (unsigned)J->traces,
                lua_tostring(L, -1));
        lua_pop(L, 1);
      }
      lua_settop(L, top);
    }
  }
  if (J->enabled) {
    int ok = try_compile(L, p, target, off, base);
    if (!ok || (p->luar_code[0] != NULL && p->luar_code[1] != NULL))
      p->luar_hot = p->luar_hot_limit + 1;
  }
  return 1;
}


/* ---------------------------------------------------------------- */
/* jit.* API                                                         */
/* ---------------------------------------------------------------- */

static LuaR_JIT *checkjit (lua_State *L) {
  LuaR_JIT *J = G(L)->luar_jit;
  if (J == NULL) {
    luari_jit_init(L);
    J = G(L)->luar_jit;
  }
  return J;
}


static int jit_on (lua_State *L) {
  LuaR_JIT *J = checkjit(L);
  if (J != NULL) J->enabled = 1;
  return 0;
}


static int jit_off (lua_State *L) {
  LuaR_JIT *J = checkjit(L);
  if (J != NULL) J->enabled = 0;
  return 0;
}


static int jit_flush (lua_State *L) {
  LuaR_JIT *J = checkjit(L);
  if (J != NULL) {
    J->epoch++;
    J->hotloops = 0;
    J->traces = 0;
    J->aborts = 0;
    free_code(J);
  }
  return 0;
}


static int jit_dump (lua_State *L) {
  LuaR_JIT *J = checkjit(L);
  const char *mode = luaL_optstring(L, 1, NULL);
  if (J != NULL) {
    J->dump_trace = (mode != NULL && strchr(mode, 't') != NULL);
    J->dump_ir = (mode != NULL && strchr(mode, 'i') != NULL);
  }
  return 0;
}


static int jit_status (lua_State *L) {
  LuaR_JIT *J = G(L)->luar_jit;
  int on = luar_has_jit() && J != NULL && J->enabled;
  lua_pushboolean(L, on);
  if (on)
    lua_pushstring(L, luar_arch());
  return on ? 2 : 1;
}


static int jit_opt_start (lua_State *L) {
  /* Accept and keep LuaJIT-style option strings.  LuaR's experimental
  ** table JIT is opt-in ("table=1") because it is not yet a performance
  ** win on the reference corpus.
  */
  LuaR_JIT *J = checkjit(L);
  int i;
  int n = lua_gettop(L);
  lua_newtable(L);
  for (i = 1; i <= n; i++) {
    const char *opt = luaL_checkstring(L, i);
    if (J != NULL && strncmp(opt, "table=", 6) == 0)
      J->enable_table = (opt[6] != '0');
    else if (J != NULL && strncmp(opt, "call=", 5) == 0)
      J->enable_partial = (opt[5] != '0');
    else if (J != NULL && strncmp(opt, "hotloop=", 8) == 0) {
      unsigned long v = strtoul(opt + 8, NULL, 10);
      if (v >= 1 && v <= 100000)
        J->hotloop = (uint32_t)v;
    }
    lua_pushstring(L, opt);
    lua_rawseti(L, -2, i);
  }
  lua_setfield(L, LUA_REGISTRYINDEX, "luar.jit.options");
  return 0;
}


static void pushtrace (lua_State *L, const LuaR_Trace *t) {
  luaL_Buffer b;
  uint32_t i;
  luaL_buffinit(L, &b);
  for (i = 0; i < t->nins; i++) {
    Instruction ins = t->code[i];
    OpCode op = GET_OPCODE(ins);
    const char *name = (op < NUM_OPCODES) ? opnames[op] : "?";
    char line[128];
    snprintf(line, sizeof(line), "%4u  %-12s A=%d B=%d C=%d Bx=%d\n",
             (unsigned)(t->startpc + i), name,
             (int)GETARG_A(ins), (int)GETARG_B(ins), (int)GETARG_C(ins),
             (int)GETARG_Bx(ins));
    luaL_addstring(&b, line);
  }
  luaL_pushresult(&b);
}


static void pushir (lua_State *L, const LuaR_Trace *t) {
  luaL_Buffer b;
  uint32_t i;
  luaL_buffinit(L, &b);
  {
    char head[96];
    snprintf(head, sizeof(head), "; trace pc %u..%u, %u instructions\n",
             (unsigned)t->startpc, (unsigned)t->endpc, (unsigned)t->nins);
    luaL_addstring(&b, head);
  }
  for (i = 0; i < t->nins; i++) {
    Instruction ins = t->code[i];
    OpCode op = GET_OPCODE(ins);
    unsigned pc = t->startpc + i;
    char line[160];
    int A = GETARG_A(ins);
    int B = GETARG_B(ins);
    int C = GETARG_C(ins);
    switch (op) {
      case OP_MOVE:
        snprintf(line, sizeof(line), "%4u  R%d = R%d\n", pc, A, B);
        break;
      case OP_LOADI:
        snprintf(line, sizeof(line), "%4u  R%d = int %d\n", pc, A,
                 (int)GETARG_sBx(ins));
        break;
      case OP_LOADF:
        snprintf(line, sizeof(line), "%4u  R%d = flt %d\n", pc, A,
                 (int)GETARG_sBx(ins));
        break;
      case OP_ADD:
        snprintf(line, sizeof(line), "%4u  R%d = R%d + R%d\n", pc, A, B, C);
        break;
      case OP_SUB:
        snprintf(line, sizeof(line), "%4u  R%d = R%d - R%d\n", pc, A, B, C);
        break;
      case OP_MUL:
        snprintf(line, sizeof(line), "%4u  R%d = R%d * R%d\n", pc, A, B, C);
        break;
      case OP_ADDI:
        snprintf(line, sizeof(line), "%4u  R%d = R%d + %d\n", pc, A, B,
                 (int)GETARG_sC(ins));
        break;
      case OP_MMBIN:
        snprintf(line, sizeof(line),
                 "%4u  ; metamethod fallback for R%d (never compiled)\n",
                 pc, A);
        break;
      case OP_FORLOOP:
        snprintf(line, sizeof(line),
                 "%4u  if (--count(R%d) != 0) goto %u; "
                 "R%d = R%d + R%d\n",
                 pc, A, (unsigned)(pc - GETARG_Bx(ins) + 1),
                 A + 2, A + 2, A + 1);
        break;
      case OP_JMP:
        snprintf(line, sizeof(line), "%4u  goto %d\n", pc,
                 (int)(pc + 1 + GETARG_sJ(ins)));
        break;
      default: {
        const char *name = (op < NUM_OPCODES) ? opnames[op] : "?";
        snprintf(line, sizeof(line), "%4u  %s A=%d B=%d C=%d\n",
                 pc, name, A, B, C);
        break;
      }
    }
    luaL_addstring(&b, line);
  }
  luaL_pushresult(&b);
}


static int jit_ir (lua_State *L) {
  LuaR_JIT *J = checkjit(L);
  lua_Integer i = luaL_checkinteger(L, 1);
  if (J == NULL || i < 1 || i > (lua_Integer)J->traces)
    return luaL_error(L, "jit.ir: no trace %d", (int)i);
  pushir(L, &J->trace[i - 1]);
  return 1;
}


static int jit_trace (lua_State *L) {
  LuaR_JIT *J = checkjit(L);
  lua_Integer i = luaL_checkinteger(L, 1);
  if (J == NULL || i < 1 || i > (lua_Integer)J->traces)
    return luaL_error(L, "jit.trace: no trace %d", (int)i);
  pushtrace(L, &J->trace[i - 1]);
  return 1;
}


static int jit_traces (lua_State *L) {
  LuaR_JIT *J = checkjit(L);
  uint32_t i;
  lua_createtable(L, J != NULL ? (int)J->traces : 0, 0);
  if (J != NULL) {
    for (i = 0; i < J->traces; i++) {
      pushtrace(L, &J->trace[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
  }
  return 1;
}


static int jit_stats (lua_State *L) {
  LuaR_JIT *J = checkjit(L);
  lua_createtable(L, 0, 8);
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->hotloops : 0);
  lua_setfield(L, -2, "hotloops");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->traces : 0);
  lua_setfield(L, -2, "traces");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->aborts : 0);
  lua_setfield(L, -2, "aborts");
  lua_pushboolean(L, J != NULL && J->enabled);
  lua_setfield(L, -2, "enabled");
  lua_pushinteger(L, (lua_Integer)J->mcode);
  lua_setfield(L, -2, "mcode");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->compiled : 0);
  lua_setfield(L, -2, "compiled");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->exits : 0);
  lua_setfield(L, -2, "exits");
  lua_pushboolean(L, luar_has_jit() && J != NULL && J->enabled);
  lua_setfield(L, -2, "status");
  return 1;
}


static const luaL_Reg jitlib[] = {
  {"on", jit_on},
  {"off", jit_off},
  {"flush", jit_flush},
  {"dump", jit_dump},
  {"status", jit_status},
  {"stats", jit_stats},
  {"trace", jit_trace},
  {"ir", jit_ir},
  {"traces", jit_traces},
  {NULL, NULL}
};


static const luaL_Reg jitoptlib[] = {
  {"start", jit_opt_start},
  {NULL, NULL}
};


LUALIB_API int luaopen_jit (lua_State *L) {
  luaL_checkversion(L);
  luari_jit_init(L);
  luaL_newlib(L, jitlib);
  lua_pushliteral(L, "LuaR " LUAR_VERSION);
  lua_setfield(L, -2, "version");
  lua_pushinteger(L, LUAR_VERSION_NUM);
  lua_setfield(L, -2, "version_num");
  lua_pushstring(L, luar_os());
  lua_setfield(L, -2, "os");
  lua_pushstring(L, luar_arch());
  lua_setfield(L, -2, "arch");
  luaL_newlib(L, jitoptlib);
  lua_setfield(L, -2, "opt");
  return 1;
}
