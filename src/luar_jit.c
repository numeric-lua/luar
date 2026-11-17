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

#include "lopcodes.h"
#include "lopnames.h"
#include "luar_emit.h"
#include "luar_jit.h"


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
  int for_a, has_branch = 0, is_while = 0;
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
    Instruction prev;
    /* A conditional backedge means the trace covers only the taken path
    ** (e.g. a break/conditional continue falls outside it); reject. */
    if (backedge <= 0)
      return 0;
    prev = p->code[backedge - 1];
    if (GET_OPCODE(prev) == OP_EQ || GET_OPCODE(prev) == OP_LT ||
        GET_OPCODE(prev) == OP_LE || GET_OPCODE(prev) == OP_EQK ||
        GET_OPCODE(prev) == OP_EQI || GET_OPCODE(prev) == OP_LTI ||
        GET_OPCODE(prev) == OP_LEI || GET_OPCODE(prev) == OP_GTI ||
        GET_OPCODE(prev) == OP_GEI)
      return 0;
    is_while = 1;
    for_a = -1;
  }
  else
    return 0;

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
      if (j >= n - 1) return 0;
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
      if (at == 0) return 0;
      if (bb >= 0) {
        bt = slot_type(b, cur, def, bb);
        if (bt == 0) return 0;
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
        if (tb == 0) return 0;
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
        if (tb == 0 || tc == 0) return 0;
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
        if (tb == 0) return 0;
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
      default:
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
  if (has_branch) {
    /* Numeric branch loops: guard every register read with the flavour the
    ** emitted code expects (a skipped write must not be mistaken for a
    ** value that exists on all paths). */
    memset(seen, 0, sizeof(seen));
    d.nguards = 0;
    for (i = 0; i < n - 1; i++) {
      LuaR_Op *op = &ops[i];
      switch (op->kind) {
        case LUAR_OP_MOVE:
          add_guard(&d, seen, op->b, op->tb);
          break;
        case LUAR_OP_ADD:
        case LUAR_OP_SUB:
        case LUAR_OP_MUL:
        case LUAR_OP_DIV:
          add_guard(&d, seen, op->b, op->tb);
          add_guard(&d, seen, op->c, op->tc);
          break;
        case LUAR_OP_ADDI:
          add_guard(&d, seen, op->b, LUAR_T_INT);
          break;
        case LUAR_OP_ADDK:
        case LUAR_OP_SUBK:
        case LUAR_OP_MULK:
        case LUAR_OP_DIVK:
          add_guard(&d, seen, op->b, op->tb);
          break;
        case LUAR_OP_LT:
        case LUAR_OP_LE:
        case LUAR_OP_GT:
        case LUAR_OP_GE:
        case LUAR_OP_EQ:
          add_guard(&d, seen, op->a, op->ta);
          if (op->b != 0xFF)
            add_guard(&d, seen, op->b, op->tb);
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
  p->luar_code = (void *)fn;
  p->luar_code_pc = (int)backedge;
  p->luar_code_base = for_a;
  p->luar_code_epoch = J->epoch;
  return 1;
}


int luari_jit_run (lua_State *L, Proto *p, void *base) {
  LuaR_JIT *J = G(L)->luar_jit;
  if (p == NULL || p->luar_code == NULL)
    return 0;
  if (J == NULL || !J->enabled)
    return 0;
  if (p->luar_code_epoch != J->epoch) {
    p->luar_code = NULL;   /* flushed: mapping was released */
    return 0;
  }
  if (L->hookmask != 0)
    return 0;              /* debug hooks need the interpreter */
  return ((LuaR_LoopFn)p->luar_code)(base);
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
  }
  if (J->enabled)
    try_compile(L, p, target, off, base);
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


static int jit_status (lua_State *L) {
  LuaR_JIT *J = G(L)->luar_jit;
  int on = luar_has_jit() && J != NULL && J->enabled;
  lua_pushboolean(L, on);
  if (on)
    lua_pushstring(L, luar_arch());
  return on ? 2 : 1;
}


static int jit_opt_start (lua_State *L) {
  /* Accept and validate the shape of LuaJIT-style option strings.  The
  ** values are kept for the future tier-1 engine in the registry.
  */
  int i;
  int n = lua_gettop(L);
  lua_newtable(L);
  for (i = 1; i <= n; i++) {
    const char *opt = luaL_checkstring(L, i);
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
  lua_pushboolean(L, luar_has_jit() && J != NULL && J->enabled);
  lua_setfield(L, -2, "status");
  return 1;
}


static const luaL_Reg jitlib[] = {
  {"on", jit_on},
  {"off", jit_off},
  {"flush", jit_flush},
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
