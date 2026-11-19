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
#include "lstring.h"
#include "lopcodes.h"
#include "lopnames.h"
#include "luar_emit.h"
#include "luar_jit.h"
#include "luar_trace.h"

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
  J->hotexit = 10;
  J->enable_tint = 1;
  J->enable_ir = 0;
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
  J->side_traces = 0;
  J->last_exit_pc = -1;
  J->sig_hits = 0;
  J->ir_executed = 0;
  J->ir_side_compiled = 0;
  J->ir_side_executed = 0;
  J->ir_call_exits = 0;
  J->ir_side_start = -1;
  J->ir_root_start = -1;
  J->ir_yields = 0;
  J->ir_side_depth = 0;
  J->ir_traces = 0;
  J->ir_ops = 0;
  J->ir_aborts = 0;
  J->ir_side_traces = 0;
  J->ir_side_ops = 0;
  J->ir_side_aborts = 0;
  J->ir_max_depth = 0;
  J->ir_cycles = 0;
  J->ir_compiled = 0;
  J->ir_code_bytes = 0;
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


static int install_keyed_sum (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                             int for_a, int table, int keys, int upval,
                             int acc) {
  int slot = -1, k;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 4;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = table;
  p->luar_tint_keys[slot] = keys;
  p->luar_tint_upval[slot] = upval;
  p->luar_tint_acc[slot] = acc;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


static int install_leaf4_intrinsic (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t backedge, int for_a,
                                   int func_slot, int acc_slot,
                                   int arg2_slot, int arg3_slot,
                                   int arg4_slot) {
  int slot = -1, k;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 9;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = func_slot;
  p->luar_tint_keys[slot] = arg2_slot;
  p->luar_tint_upval[slot] = arg3_slot;
  p->luar_tint_arg4[slot] = arg4_slot;
  p->luar_tint_acc[slot] = acc_slot;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


static int install_leaf3_intrinsic (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t backedge, int for_a,
                                   int func_slot, int acc_slot,
                                   int arg2_slot, int arg3_slot) {
  int slot = -1, k;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 8;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = func_slot;
  p->luar_tint_keys[slot] = arg2_slot;
  p->luar_tint_upval[slot] = arg3_slot;
  p->luar_tint_acc[slot] = acc_slot;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


static int install_leaf2_intrinsic (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t backedge, int for_a,
                                   int func_slot, int acc_slot,
                                   int arg2_slot) {
  int slot = -1, k;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 7;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = func_slot;
  p->luar_tint_keys[slot] = arg2_slot;
  p->luar_tint_upval[slot] = -1;
  p->luar_tint_acc[slot] = acc_slot;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


static int install_leaf_intrinsic (Proto *p, LuaR_JIT *J,
                                  ptrdiff_t backedge, int for_a,
                                  int func_slot, int acc_slot) {
  int slot = -1, k;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 6;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = func_slot;
  p->luar_tint_keys[slot] = 0;
  p->luar_tint_upval[slot] = -1;
  p->luar_tint_acc[slot] = acc_slot;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


static int install_method_intrinsic (Proto *p, LuaR_JIT *J,
                                    ptrdiff_t backedge, int for_a,
                                    int table, int method_key, int upval) {
  int slot = -1, k;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 5;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = table;
  p->luar_tint_keys[slot] = method_key;
  p->luar_tint_upval[slot] = upval;
  p->luar_tint_acc[slot] = 0;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


static int install_keyed_fill (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                              int for_a, int table, int keys, int upval) {
  int slot = -1, k;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 3;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = table;
  p->luar_tint_keys[slot] = keys;
  p->luar_tint_upval[slot] = upval;
  p->luar_tint_acc[slot] = 0;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


static int match_closure_loop (Proto *p, ptrdiff_t start,
                               ptrdiff_t backedge, int for_a, TValue *base,
                               LuaR_LoopDesc *d) {
  Instruction m0, m1, call, f0, f1, f2, f3, f4, f5, f6;
  int call_a, func_slot = -1, arg_slot = -1;
  LClosure *cl;
  UpVal *uv;
  ptrdiff_t off;
  if (backedge != start + 3)
    return 0;
  m0 = p->code[start];
  m1 = p->code[start + 1];
  call = p->code[start + 2];
  if (GET_OPCODE(m0) != OP_MOVE || GET_OPCODE(m1) != OP_MOVE ||
      GET_OPCODE(call) != OP_CALL)
    return 0;
  if (GETARG_B(call) != 2 || GETARG_C(call) != 1)
    return 0;
  call_a = GETARG_A(call);
  if (GETARG_A(m0) == call_a)
    func_slot = GETARG_B(m0);
  if (GETARG_A(m1) == call_a)
    func_slot = GETARG_B(m1);
  if (GETARG_A(m1) == call_a + 1)
    arg_slot = GETARG_B(m1);
  if (GETARG_A(m0) == call_a + 1)
    arg_slot = GETARG_B(m0);
  if (func_slot < 0 || arg_slot != for_a + 2)
    return 0;
  if (!ttisLclosure(&base[func_slot]))
    return 0;
  cl = clLvalue(&base[func_slot]);
  if (cl->p->numparams != 1 || cl->p->sizeupvalues != 1 ||
      cl->p->sizecode < 7 || cl->p->maxstacksize < 2)
    return 0;
  f0 = cl->p->code[0]; f1 = cl->p->code[1]; f2 = cl->p->code[2];
  f3 = cl->p->code[3]; f4 = cl->p->code[4]; f5 = cl->p->code[5];
  f6 = cl->p->code[6];
  if (GET_OPCODE(f0) != OP_GETUPVAL || GETARG_A(f0) != 1 ||
      GETARG_B(f0) != 0 ||
      GET_OPCODE(f1) != OP_ADD || GETARG_A(f1) != 1 ||
      GETARG_B(f1) != 1 || GETARG_C(f1) != 0 ||
      GET_OPCODE(f2) != OP_MMBIN ||
      GET_OPCODE(f3) != OP_SETUPVAL || GETARG_A(f3) != 1 ||
      GETARG_B(f3) != 0 ||
      GET_OPCODE(f4) != OP_GETUPVAL || GETARG_A(f4) != 1 ||
      GETARG_B(f4) != 0 ||
      GET_OPCODE(f5) != OP_RETURN1 ||
      GET_OPCODE(f6) != OP_RETURN0)
    return 0;
  uv = cl->upvals[0];
  off = uv->v.p - base;
  if (off < 0 || off >= p->maxstacksize || off > 255)
    return 0;
  if (!ttisinteger(uv->v.p))
    return 0;
  d->fast_closure = 1;
  d->fc_func_slot = func_slot;
  d->fc_upval_slot = (int)off;
  d->fc_closure = cl->p;
  return 1;
}


static int meta_multiplier (lua_State *L, TValue *tt, int64_t *mult) {
  TValue fn;
  Table *t, *mt;
  LClosure *cl;
  TString *key;
  if (!ttistable(tt))
    return 0;
  t = hvalue(tt);
  if (t->asize != 0 || t->lsizenode != 0 || t->metatable == NULL)
    return 0;
  mt = t->metatable;
  key = luaS_newliteral(L, "__index");
  if (luaH_getstr(mt, key, &fn) != LUA_VLCL)
    return 0;
  cl = clLvalue(&fn);
  if (cl->p->numparams != 2 || cl->p->sizeupvalues != 0 ||
      cl->p->sizecode < 4 || cl->p->maxstacksize < 3)
    return 0;
  if (GET_OPCODE(cl->p->code[0]) != OP_MULK ||
      GETARG_A(cl->p->code[0]) != 2 || GETARG_B(cl->p->code[0]) != 1 ||
      GET_OPCODE(cl->p->code[1]) != OP_MMBINK ||
      GET_OPCODE(cl->p->code[2]) != OP_RETURN1 ||
      GETARG_A(cl->p->code[2]) != 2 ||
      GET_OPCODE(cl->p->code[3]) != OP_RETURN0)
    return 0;
  {
    TValue *k = &cl->p->k[GETARG_C(cl->p->code[0])];
    if (!ttisinteger(k))
      return 0;
    *mult = ivalue(k);
  }
  return 1;
}


static int match_meta_loop (lua_State *L, Proto *p, ptrdiff_t start,
                            ptrdiff_t backedge, int for_a, TValue *base,
                            LuaR_LoopDesc *d) {
  Instruction gt, add, mm, fp;
  int64_t mult;
  if (backedge != start + 3)
    return 0;
  gt = p->code[start]; add = p->code[start + 1];
  mm = p->code[start + 2]; fp = p->code[start + 3];
  if (GET_OPCODE(gt) != OP_GETTABLE || GET_OPCODE(add) != OP_ADD ||
      GET_OPCODE(mm) != OP_MMBIN || GET_OPCODE(fp) != OP_FORLOOP)
    return 0;
  if (GETARG_C(gt) != for_a + 2 || GETARG_C(add) != GETARG_A(gt) ||
      GETARG_A(add) != GETARG_B(add) || GETARG_A(fp) != for_a)
    return 0;
  if (!ttisinteger(&base[GETARG_A(add)]) ||
      GETARG_A(gt) == for_a + 2)
    return 0;
  if (!meta_multiplier(L, &base[GETARG_B(gt)], &mult))
    return 0;
  d->fast_meta = 1;
  d->fm_table = GETARG_B(gt);
  d->fm_acc = GETARG_A(add);
  d->fm_upval = -1;
  d->fm_mult = mult;
  return 1;
}


static int match_meta_upval_loop (lua_State *L, Proto *p, ptrdiff_t start,
                                  ptrdiff_t backedge, int for_a, TValue *base,
                                  LuaR_LoopDesc *d) {
  Instruction up, gt, add, mm, fp;
  TValue *fv, *tab;
  LClosure *cl;
  UpVal *uv;
  int64_t mult;
  if (backedge != start + 4)
    return 0;
  up = p->code[start]; gt = p->code[start + 1];
  add = p->code[start + 2]; mm = p->code[start + 3];
  fp = p->code[start + 4];
  if (GET_OPCODE(up) != OP_GETUPVAL || GET_OPCODE(gt) != OP_GETTABLE ||
      GET_OPCODE(add) != OP_ADD || GET_OPCODE(mm) != OP_MMBIN ||
      GET_OPCODE(fp) != OP_FORLOOP)
    return 0;
  if (GETARG_A(up) != GETARG_B(gt) || GETARG_C(gt) != for_a + 2 ||
      GETARG_C(add) != GETARG_A(gt) || GETARG_A(add) != GETARG_B(add) ||
      GETARG_A(fp) != for_a)
    return 0;
  if (!ttisinteger(&base[GETARG_A(add)]) ||
      GETARG_A(gt) == for_a + 2)
    return 0;
  fv = &base[-1];
  if (!ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  if (GETARG_B(up) >= cl->p->sizeupvalues)
    return 0;
  uv = cl->upvals[GETARG_B(up)];
  tab = uv->v.p;
  if (!meta_multiplier(L, tab, &mult))
    return 0;
  d->fast_meta = 1;
  d->fm_table = 0;
  d->fm_acc = GETARG_A(add);
  d->fm_upval = GETARG_B(up);
  d->fm_mult = mult;
  return 1;
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

  if (!is_while && for_a >= 0) {
    Instruction i0 = p->code[start];
    if (J->enable_tint && n >= 3 && GET_OPCODE(i0) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 1]) == OP_SETTABLE &&
        backedge == start + 2) {
      Instruction i1 = p->code[start + 1];
      if (GETARG_C(i0) == for_a + 2 &&
          GETARG_B(i1) == GETARG_A(i0) &&
          GETARG_C(i1) == for_a + 2)
        return install_keyed_fill(p, J, backedge, for_a,
                                  GETARG_A(i1), GETARG_B(i0), -1);
    }
    if (J->enable_tint && n >= 4 && GET_OPCODE(i0) == OP_GETUPVAL &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 2]) == OP_SETTABLE &&
        backedge == start + 3) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      if (GETARG_A(i0) == GETARG_B(i1) &&
          GETARG_C(i1) == for_a + 2 &&
          GETARG_B(i2) == GETARG_A(i1) &&
          GETARG_C(i2) == for_a + 2)
        return install_keyed_fill(p, J, backedge, for_a,
                                  GETARG_A(i2), 0, GETARG_B(i0));
    }
    if (J->enable_tint && n >= 5 && GET_OPCODE(i0) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 2]) == OP_ADD &&
        GET_OPCODE(p->code[start + 3]) == OP_MMBIN &&
        backedge == start + 4) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      if (GETARG_C(i0) == for_a + 2 &&
          GETARG_C(i1) == GETARG_A(i0) &&
          GETARG_A(i1) == GETARG_A(i0) &&
          GETARG_A(i2) == GETARG_B(i2) &&
          GETARG_C(i2) == GETARG_A(i1))
        return install_keyed_sum(p, J, backedge, for_a,
                                 GETARG_B(i1), GETARG_B(i0), -1,
                                 GETARG_A(i2));
    }
    if (J->enable_tint && n >= 6 && GET_OPCODE(i0) == OP_GETUPVAL &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 2]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 3]) == OP_ADD &&
        GET_OPCODE(p->code[start + 4]) == OP_MMBIN &&
        backedge == start + 5) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      Instruction i3 = p->code[start + 3];
      if (GETARG_A(i0) == GETARG_B(i1) &&
          GETARG_C(i1) == for_a + 2 &&
          GETARG_C(i2) == GETARG_A(i1) &&
          GETARG_A(i2) == GETARG_A(i1) &&
          GETARG_A(i3) == GETARG_B(i3) &&
          GETARG_C(i3) == GETARG_A(i2))
        return install_keyed_sum(p, J, backedge, for_a,
                                 GETARG_B(i2), 0, GETARG_B(i0), GETARG_A(i3));
    }
    if (n >= 9 && GET_OPCODE(i0) == OP_MOVE &&
        GET_OPCODE(p->code[start + 1]) == OP_MOVE &&
        (GET_OPCODE(p->code[start + 2]) == OP_LOADI ||
         GET_OPCODE(p->code[start + 2]) == OP_LOADK ||
         GET_OPCODE(p->code[start + 2]) == OP_MOVE) &&
        (GET_OPCODE(p->code[start + 3]) == OP_LOADI ||
         GET_OPCODE(p->code[start + 3]) == OP_LOADK ||
         GET_OPCODE(p->code[start + 3]) == OP_MOVE) &&
        (GET_OPCODE(p->code[start + 4]) == OP_LOADI ||
         GET_OPCODE(p->code[start + 4]) == OP_LOADK ||
         GET_OPCODE(p->code[start + 4]) == OP_MOVE) &&
        GET_OPCODE(p->code[start + 5]) == OP_CALL &&
        GET_OPCODE(p->code[start + 6]) == OP_ADD &&
        GET_OPCODE(p->code[start + 7]) == OP_MMBIN &&
        backedge == start + 8) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      Instruction i3 = p->code[start + 3];
      Instruction i4 = p->code[start + 4];
      Instruction i5 = p->code[start + 5];
      Instruction i6 = p->code[start + 6];
      if (GETARG_A(i1) == GETARG_A(i0) + 1 &&
          GETARG_B(i1) == for_a + 2 &&
          GETARG_A(i2) == GETARG_A(i0) + 2 &&
          GETARG_A(i3) == GETARG_A(i0) + 3 &&
          GETARG_A(i4) == GETARG_A(i0) + 4 &&
          GETARG_A(i5) == GETARG_A(i0) &&
          GETARG_B(i5) == 5 && GETARG_C(i5) == 2 &&
          GETARG_A(i6) == GETARG_B(i6) &&
          GETARG_C(i6) == GETARG_A(i5))
        return install_leaf4_intrinsic(p, J, backedge, for_a,
                                       GETARG_B(i0), GETARG_A(i6),
                                       GETARG_A(i2), GETARG_A(i3),
                                       GETARG_A(i4));
    }
    if (n >= 8 && GET_OPCODE(i0) == OP_MOVE &&
        GET_OPCODE(p->code[start + 1]) == OP_MOVE &&
        (GET_OPCODE(p->code[start + 2]) == OP_LOADI ||
         GET_OPCODE(p->code[start + 2]) == OP_LOADK ||
         GET_OPCODE(p->code[start + 2]) == OP_MOVE) &&
        (GET_OPCODE(p->code[start + 3]) == OP_LOADI ||
         GET_OPCODE(p->code[start + 3]) == OP_LOADK ||
         GET_OPCODE(p->code[start + 3]) == OP_MOVE) &&
        GET_OPCODE(p->code[start + 4]) == OP_CALL &&
        GET_OPCODE(p->code[start + 5]) == OP_ADD &&
        GET_OPCODE(p->code[start + 6]) == OP_MMBIN &&
        backedge == start + 7) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      Instruction i3 = p->code[start + 3];
      Instruction i4 = p->code[start + 4];
      Instruction i5 = p->code[start + 5];
      if (GETARG_A(i1) == GETARG_A(i0) + 1 &&
          GETARG_B(i1) == for_a + 2 &&
          GETARG_A(i2) == GETARG_A(i0) + 2 &&
          GETARG_A(i3) == GETARG_A(i0) + 3 &&
          GETARG_A(i4) == GETARG_A(i0) &&
          GETARG_B(i4) == 4 && GETARG_C(i4) == 2 &&
          GETARG_A(i5) == GETARG_B(i5) &&
          GETARG_C(i5) == GETARG_A(i4))
        return install_leaf3_intrinsic(p, J, backedge, for_a,
                                       GETARG_B(i0), GETARG_A(i5),
                                       GETARG_A(i2), GETARG_A(i3));
    }
    if (n >= 7 && GET_OPCODE(i0) == OP_MOVE &&
        GET_OPCODE(p->code[start + 1]) == OP_MOVE &&
        (GET_OPCODE(p->code[start + 2]) == OP_LOADI ||
         GET_OPCODE(p->code[start + 2]) == OP_LOADK ||
         GET_OPCODE(p->code[start + 2]) == OP_MOVE) &&
        GET_OPCODE(p->code[start + 3]) == OP_CALL &&
        GET_OPCODE(p->code[start + 4]) == OP_ADD &&
        GET_OPCODE(p->code[start + 5]) == OP_MMBIN &&
        backedge == start + 6) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      Instruction i3 = p->code[start + 3];
      Instruction i4 = p->code[start + 4];
      if (GETARG_A(i1) == GETARG_A(i0) + 1 &&
          GETARG_B(i1) == for_a + 2 &&
          GETARG_A(i2) == GETARG_A(i0) + 2 &&
          GETARG_A(i3) == GETARG_A(i0) &&
          GETARG_B(i3) == 3 && GETARG_C(i3) == 2 &&
          GETARG_A(i4) == GETARG_B(i4) &&
          GETARG_C(i4) == GETARG_A(i3))
        return install_leaf2_intrinsic(p, J, backedge, for_a,
                                       GETARG_B(i0), GETARG_A(i4),
                                       GETARG_A(i2));
    }
    if (n >= 6 && GET_OPCODE(i0) == OP_MOVE &&
        GET_OPCODE(p->code[start + 1]) == OP_MOVE &&
        GET_OPCODE(p->code[start + 2]) == OP_CALL &&
        GET_OPCODE(p->code[start + 3]) == OP_ADD &&
        GET_OPCODE(p->code[start + 4]) == OP_MMBIN &&
        backedge == start + 5) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      Instruction i3 = p->code[start + 3];
      if (GETARG_A(i1) == GETARG_A(i0) + 1 &&
          GETARG_B(i1) == for_a + 2 &&
          GETARG_A(i2) == GETARG_A(i0) &&
          GETARG_B(i2) == 2 && GETARG_C(i2) == 2 &&
          GETARG_A(i3) == GETARG_B(i3) &&
          GETARG_C(i3) == GETARG_A(i2))
        return install_leaf_intrinsic(p, J, backedge, for_a,
                                      GETARG_B(i0), GETARG_A(i3));
    }
    if (n >= 5 && GET_OPCODE(i0) == OP_GETUPVAL &&
        GET_OPCODE(p->code[start + 1]) == OP_SELF &&
        GET_OPCODE(p->code[start + 2]) == OP_MOVE &&
        GET_OPCODE(p->code[start + 3]) == OP_CALL &&
        backedge == start + 4) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      Instruction i3 = p->code[start + 3];
      if (GETARG_A(i0) == GETARG_B(i1) &&
          GETARG_A(i1) == GETARG_A(i0) &&
          GETARG_A(i2) == GETARG_A(i1) + 2 &&
          GETARG_B(i2) == for_a + 2 &&
          GETARG_A(i3) == GETARG_A(i1) &&
          GETARG_B(i3) == 3 && GETARG_C(i3) == 1)
        return install_method_intrinsic(p, J, backedge, for_a,
                                        0, GETARG_C(i1), GETARG_B(i0));
    }
    if (n >= 4 && GET_OPCODE(i0) == OP_SELF &&
        GET_OPCODE(p->code[start + 1]) == OP_MOVE &&
        GET_OPCODE(p->code[start + 2]) == OP_CALL &&
        backedge == start + 3) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      if (GETARG_A(i1) == GETARG_A(i0) + 2 &&
          GETARG_B(i1) == for_a + 2 &&
          GETARG_A(i2) == GETARG_A(i0) &&
          GETARG_B(i2) == 3 && GETARG_C(i2) == 1) {
        return install_method_intrinsic(p, J, backedge, for_a,
                                        GETARG_B(i0), GETARG_C(i0), -1);
      }
    }
  }
  for (i = 0; i < n - 1; i++) {
    if (GET_OPCODE(p->code[start + i]) == OP_CALL) {
      call_at = (int)i;
      break;
    }
  }
  memset(&d, 0, sizeof(d));
  d.entry_pc = (int)start;
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

  if (!is_while && for_a >= 0 &&
      match_meta_upval_loop(L, p, start, backedge, for_a, b, &d))
    goto emit_ready;
  if (!is_while && for_a >= 0 &&
      match_meta_loop(L, p, start, backedge, for_a, b, &d))
    goto emit_ready;
  if (!is_while && for_a >= 0 &&
      match_closure_loop(p, start, backedge, for_a, b, &d))
    goto emit_ready;
  for (i = 0; i < n - 1; i++) {
    Instruction ins = p->code[start + i];
    LuaR_Op *op = &ops[i];
    op->pc = (int)(start + i);
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
  if ((d.fast_fill || d.fast_tsum) && J->enable_tint) {
    int slot = -1, k;
    int tint = d.fast_fill ? 1 : 2;
    for (k = 0; k < 2; k++) {
      if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
        slot = k;
        break;
      }
    }
    if (slot < 0) {
      for (k = 0; k < 2; k++) {
        if (p->luar_code_pc[k] == (int)backedge ||
            (p->luar_tint[k] != 0 && p->luar_tint_pc[k] == (int)backedge)) {
          slot = k;
          break;
        }
      }
    }
    if (slot < 0)
      slot = 1;
    p->luar_code[slot] = NULL;
    p->luar_tint[slot] = tint;
    p->luar_tint_pc[slot] = (int)backedge;
    p->luar_tint_table[slot] = d.fast_fill ? d.ff_table : d.ft_table;
    p->luar_tint_acc[slot] = d.fast_tsum ? d.ft_acc : 0;
    p->luar_tint_base[slot] = for_a;
    p->luar_tint_epoch[slot] = J->epoch;
    J->compiled++;
    return 1;
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
                op->kind == LUAR_OP_MUL ||
                (op->kind == LUAR_OP_DIV && op->ta == LUAR_T_FLOAT))) {
        accop = op;
        acc_i = (int)i;
        opkind = (op->kind == LUAR_OP_SUB) ? 1 :
                 (op->kind == LUAR_OP_MUL) ? 2 :
                 (op->kind == LUAR_OP_DIV) ? 4 : 0;
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
      if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
        slot = k;
        break;
      }
    }
    if (slot < 0) {
      for (k = 0; k < 2; k++) {
        if (p->luar_code_pc[k] == (int)backedge ||
            (p->luar_tint[k] != 0 && p->luar_tint_pc[k] == (int)backedge)) {
          slot = k;
          break;
        }
      }
    }
    if (slot < 0)
      slot = 1;   /* keep the first loop, replace the most recent */
    p->luar_code[slot] = (void *)fn;
    p->luar_code_pc[slot] = (int)backedge;
    p->luar_code_base[slot] = for_a;
    p->luar_code_start[slot] = (int)start;
    p->luar_code_epoch[slot] = J->epoch;
    {
      int gi;
      p->luar_ngrd[slot] = (uint8_t)(d.nguards <= 8 ? d.nguards : 0);
      p->luar_sig[slot] = 1469598103934665603ull;
      for (gi = 0; gi < p->luar_ngrd[slot]; gi++) {
        p->luar_grd_reg[slot][gi] = d.guard_reg[gi];
        p->luar_grd_typ[slot][gi] = d.guard_type[gi];
        p->luar_sig[slot] = (p->luar_sig[slot] ^ d.guard_reg[gi]) *
                            1099511628211ull;
        p->luar_sig[slot] = (p->luar_sig[slot] ^ d.guard_type[gi]) *
                            1099511628211ull;
      }
    }
    p->luar_fc_closure[slot] = d.fc_closure;
    p->luar_fc_func[slot] = d.fc_func_slot;
    p->luar_fc_upval[slot] = d.fc_upval_slot;
    p->luar_meta[slot] = d.fast_meta;
    p->luar_meta_table[slot] = d.fm_table;
    p->luar_meta_acc[slot] = d.fm_acc;
    p->luar_meta_upval[slot] = d.fm_upval;
    p->luar_meta_mult[slot] = d.fm_mult;
  }
  return 1;
}


static int table_fill_intrinsic (lua_State *L, TValue *base,
                                 int for_a, int table_reg) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *tt = &base[table_reg];
  lua_Integer count, step, idx, last, k;
  Table *h;
  if (!ttistable(tt) || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  last = idx + count;
  if (last > (1 << 22))
    return 0;
  h = hvalue(tt);
  if (h->metatable != NULL)
    return 0;
  if (h->asize < (unsigned)last)
    luaH_resize(L, h, (unsigned)last, 0);
  for (k = 0; k <= count; k++) {
    TValue v;
    setivalue(&v, idx + k);
    obj2arr(h, (unsigned)(idx - 1 + k), &v);
  }
  setivalue(ct, 0);
  setivalue(it, idx + count);
  return 1;
}


static int table_sum_intrinsic (lua_State *L, TValue *base,
                                int for_a, int table_reg, int acc_reg) {
  (void)L;
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *tt = &base[table_reg];
  TValue *at = &base[acc_reg];
  lua_Integer count, step, idx, last, k;
  Table *h;
  if (!ttistable(tt) || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  last = idx + count;
  if (last > (1 << 22))
    return 0;
  h = hvalue(tt);
  if (h->metatable != NULL || (unsigned)last > h->asize)
    return 0;
  if (ttisinteger(at)) {
    lua_Unsigned sum = l_castS2U(ivalue(at));
    for (k = 0; k <= count; k++) {
      unsigned key = (unsigned)(idx + k - 1);
      lu_byte tag = *getArrTag(h, key);
      if (tag != LUA_VNUMINT)
        return 0;
      sum += l_castS2U(getArrVal(h, key)->i);
    }
    chgivalue(at, l_castU2S(sum));
  }
  else if (ttisfloat(at)) {
    lua_Number sum = fltvalue(at);
    for (k = 0; k <= count; k++) {
      unsigned key = (unsigned)(idx + k - 1);
      lu_byte tag = *getArrTag(h, key);
      if (tag == LUA_VNUMINT)
        sum += cast_num(getArrVal(h, key)->i);
      else if (tag == LUA_VNUMFLT)
        sum += getArrVal(h, key)->n;
      else
        return 0;
    }
    setfltvalue(at, sum);
  }
  else
    return 0;
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int keyed_fill_intrinsic (lua_State *L, TValue *base,
                                int for_a, int table_reg, Table *keys) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *tt = &base[table_reg];
  lua_Integer count, step, idx, last, k;
  Table *t;
  if (!ttistable(tt) || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  last = idx + count;
  t = hvalue(tt);
  if (t->metatable != NULL || keys->metatable != NULL)
    return 0;
  if (t->asize == 0)
    luaH_resize(L, t, 0, (unsigned)(count + 1));
  for (k = 0; k <= count; k++) {
    TValue key, val;
    if (luaH_getint(keys, idx + k, &key) == LUA_VNIL)
      return 0;
    setivalue(&val, idx + k);
    luaH_set(L, t, &key, &val);
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int keyed_sum_intrinsic (lua_State *L, TValue *base,
                               int for_a, int table_reg, Table *keys,
                               int acc_reg) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *tt = &base[table_reg];
  TValue *at = &base[acc_reg];
  lua_Integer count, step, idx, last, k;
  Table *t;
  (void)L;
  if (!ttistable(tt) || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  last = idx + count;
  t = hvalue(tt);
  if (t->metatable != NULL || keys->metatable != NULL)
    return 0;
  if (ttisinteger(at)) {
    lua_Unsigned sum = l_castS2U(ivalue(at));
    for (k = 0; k <= count; k++) {
      TValue key, res;
      if (luaH_getint(keys, idx + k, &key) == LUA_VNIL)
        return 0;
      if (luaH_get(t, &key, &res) != LUA_VNUMINT)
        return 0;
      sum += l_castS2U(ivalue(&res));
    }
    chgivalue(at, l_castU2S(sum));
  }
  else if (ttisfloat(at)) {
    lua_Number sum = fltvalue(at);
    for (k = 0; k <= count; k++) {
      TValue key, res;
      lu_byte tag;
      if (luaH_getint(keys, idx + k, &key) == LUA_VNIL)
        return 0;
      tag = luaH_get(t, &key, &res);
      if (tag == LUA_VNUMINT)
        sum += cast_num(ivalue(&res));
      else if (tag == LUA_VNUMFLT)
        sum += fltvalue(&res);
      else
        return 0;
    }
    setfltvalue(at, sum);
  }
  else
    return 0;
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int method_add_intrinsic (lua_State *L, Proto *caller, TValue *base,
                                int for_a, TValue *tt, int method_key) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue mval, fval, newv;
  Table *t;
  LClosure *cl;
  TValue *field_key;
  lua_Integer count, step, idx, last, k;
  lua_Unsigned sum;
  if (!ttistable(tt) || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  last = idx + count;
  t = hvalue(tt);
  if (t->metatable != NULL)
    return 0;
  if (method_key < 0 || method_key >= caller->sizek)
    return 0;
  if (luaH_get(t, &caller->k[method_key], &mval) != LUA_VLCL)
    return 0;
  cl = clLvalue(&mval);
  if (cl->p->numparams != 2 || cl->p->sizeupvalues != 0 ||
      cl->p->sizecode < 5 || cl->p->maxstacksize < 3)
    return 0;
  if (GET_OPCODE(cl->p->code[0]) != OP_GETFIELD ||
      GETARG_A(cl->p->code[0]) != 2 ||
      GET_OPCODE(cl->p->code[1]) != OP_ADD ||
      GETARG_A(cl->p->code[1]) != 2 ||
      GETARG_B(cl->p->code[1]) != 2 ||
      GETARG_C(cl->p->code[1]) != 1 ||
      GET_OPCODE(cl->p->code[2]) != OP_MMBIN ||
      GET_OPCODE(cl->p->code[3]) != OP_SETFIELD ||
      GETARG_A(cl->p->code[3]) != 0 ||
      GETARG_C(cl->p->code[3]) != 2 ||
      GET_OPCODE(cl->p->code[4]) != OP_RETURN0)
    return 0;
  field_key = &cl->p->k[GETARG_C(cl->p->code[0])];
  if (luaH_get(t, field_key, &fval) != LUA_VNUMINT)
    return 0;
  sum = l_castS2U(ivalue(&fval));
  for (k = 0; k <= count; k++)
    sum += l_castS2U(idx + k);
  setivalue(&newv, l_castU2S(sum));
  luaH_set(L, t, field_key, &newv);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static lua_Integer jit_wrap_add (lua_Integer a, lua_Integer b) {
  return l_castU2S(l_castS2U(a) + l_castS2U(b));
}


static lua_Integer jit_wrap_mul (lua_Integer a, lua_Integer b) {
  return l_castU2S(l_castS2U(a) * l_castS2U(b));
}


static int leaf_single_plan (Proto *fp, int *op, lua_Integer *imm) {
  if (fp->numparams != 1 || fp->sizeupvalues != 0 ||
      fp->sizecode < 4 || fp->maxstacksize < 2)
    return 0;
  if (GET_OPCODE(fp->code[0]) == OP_MULK &&
      GETARG_A(fp->code[0]) == 1 && GETARG_B(fp->code[0]) == 0 &&
      ttisinteger(&fp->k[GETARG_C(fp->code[0])])) {
    *op = 1;
    *imm = ivalue(&fp->k[GETARG_C(fp->code[0])]);
    if (GET_OPCODE(fp->code[1]) != OP_MMBINK)
      return 0;
  }
  else if (GET_OPCODE(fp->code[0]) == OP_ADDI &&
           GETARG_A(fp->code[0]) == 1 && GETARG_B(fp->code[0]) == 0) {
    *op = 2;
    *imm = GETARG_sC(fp->code[0]);
    if (GET_OPCODE(fp->code[1]) != OP_MMBINI &&
        GET_OPCODE(fp->code[1]) != OP_MMBIN)
      return 0;
  }
  else if (GET_OPCODE(fp->code[0]) == OP_MUL &&
           GETARG_A(fp->code[0]) == 1 && GETARG_B(fp->code[0]) == 0 &&
           GETARG_C(fp->code[0]) == 0) {
    *op = 3;
    if (GET_OPCODE(fp->code[1]) != OP_MMBIN)
      return 0;
  }
  else if (GET_OPCODE(fp->code[0]) == OP_ADD &&
           GETARG_A(fp->code[0]) == 1 && GETARG_B(fp->code[0]) == 0 &&
           GETARG_C(fp->code[0]) == 0) {
    *op = 4;
    if (GET_OPCODE(fp->code[1]) != OP_MMBIN)
      return 0;
  }
  else
    return 0;
  if (GET_OPCODE(fp->code[2]) != OP_RETURN1 ||
      GETARG_A(fp->code[2]) != 1 ||
      GET_OPCODE(fp->code[3]) != OP_RETURN0)
    return 0;
  return 1;
}


static int leaf_arm (Proto *fp, int at, int *op, lua_Integer *imm) {
  Instruction i = fp->code[at];
  if (at + 3 >= fp->sizecode)
    return 0;
  if (GET_OPCODE(i) == OP_MULK && GETARG_A(i) == 1 &&
      GETARG_B(i) == 0 && ttisinteger(&fp->k[GETARG_C(i)]) &&
      GET_OPCODE(fp->code[at + 1]) == OP_MMBINK &&
      GET_OPCODE(fp->code[at + 2]) == OP_RETURN1 &&
      GETARG_A(fp->code[at + 2]) == 1) {
    *op = 1;
    *imm = ivalue(&fp->k[GETARG_C(i)]);
    return 1;
  }
  if (GET_OPCODE(i) == OP_ADDI && GETARG_A(i) == 1 &&
      GETARG_B(i) == 0 &&
      (GET_OPCODE(fp->code[at + 1]) == OP_MMBINI ||
       GET_OPCODE(fp->code[at + 1]) == OP_MMBIN) &&
      GET_OPCODE(fp->code[at + 2]) == OP_RETURN1 &&
      GETARG_A(fp->code[at + 2]) == 1) {
    *op = 2;
    *imm = GETARG_sC(i);
    return 1;
  }
  if (GET_OPCODE(i) == OP_MUL && GETARG_A(i) == 1 &&
      GETARG_B(i) == 0 && GETARG_C(i) == 0 &&
      GET_OPCODE(fp->code[at + 1]) == OP_MMBIN &&
      GET_OPCODE(fp->code[at + 2]) == OP_RETURN1 &&
      GETARG_A(fp->code[at + 2]) == 1) {
    *op = 3;
    *imm = 0;
    return 1;
  }
  return 0;
}


static int leaf_branch_plan (Proto *fp, int *cmp, lua_Integer *threshold,
                             int *op1, lua_Integer *imm1,
                             int *op2, lua_Integer *imm2) {
  Instruction c;
  int j1, j2;
  if (fp->numparams != 1 || fp->sizeupvalues != 0 ||
      fp->sizecode != 10 || fp->maxstacksize < 2)
    return 0;
  c = fp->code[0];
  if ((GET_OPCODE(c) != OP_LTI && GET_OPCODE(c) != OP_LEI &&
       GET_OPCODE(c) != OP_GTI && GET_OPCODE(c) != OP_GEI &&
       GET_OPCODE(c) != OP_EQI) ||
      GETARG_A(c) != 0 || GETARG_k(c) != 0)
    return 0;
  if (GET_OPCODE(fp->code[1]) != OP_JMP ||
      GET_OPCODE(fp->code[5]) != OP_JMP ||
      GET_OPCODE(fp->code[9]) != OP_RETURN0)
    return 0;
  j1 = GETARG_sJ(fp->code[1]);
  j2 = GETARG_sJ(fp->code[5]);
  if (j1 != 4 || j2 != 3)
    return 0;
  if (!leaf_arm(fp, 2, op1, imm1) || !leaf_arm(fp, 6, op2, imm2))
    return 0;
  *cmp = GET_OPCODE(c);
  *threshold = GETARG_sB(c);
  return 1;
}


static lua_Integer leaf_branch_apply (int cmp, lua_Integer v, lua_Integer c,
                                      int op1, lua_Integer imm1,
                                      int op2, lua_Integer imm2) {
  int cond;
  int op;
  lua_Integer imm;
  switch (cmp) {
    case OP_LTI: cond = (v < c); break;
    case OP_LEI: cond = (v <= c); break;
    case OP_GTI: cond = (v > c); break;
    case OP_GEI: cond = (v >= c); break;
    default: cond = (v == c); break;
  }
  op = cond ? op1 : op2;
  imm = cond ? imm1 : imm2;
  if (op == 1) return jit_wrap_mul(v, imm);
  if (op == 2) return jit_wrap_add(v, imm);
  return jit_wrap_mul(v, v);
}


static int leaf_affine_plan (Proto *fp, lua_Integer *mul, lua_Integer *add) {
  if (fp->numparams != 1 || fp->sizeupvalues != 0 ||
      fp->sizecode < 6 || fp->maxstacksize < 2)
    return 0;
  if (GET_OPCODE(fp->code[0]) == OP_MULK &&
      GETARG_A(fp->code[0]) == 1 && GETARG_B(fp->code[0]) == 0 &&
      ttisinteger(&fp->k[GETARG_C(fp->code[0])]) &&
      GET_OPCODE(fp->code[1]) == OP_MMBINK &&
      GET_OPCODE(fp->code[2]) == OP_ADDI &&
      GETARG_A(fp->code[2]) == 1 && GETARG_B(fp->code[2]) == 1 &&
      (GET_OPCODE(fp->code[3]) == OP_MMBINI ||
       GET_OPCODE(fp->code[3]) == OP_MMBIN) &&
      GET_OPCODE(fp->code[4]) == OP_RETURN1 &&
      GETARG_A(fp->code[4]) == 1 &&
      GET_OPCODE(fp->code[5]) == OP_RETURN0) {
    *mul = ivalue(&fp->k[GETARG_C(fp->code[0])]);
    *add = GETARG_sC(fp->code[2]);
    return 1;
  }
  if (GET_OPCODE(fp->code[0]) == OP_ADDI &&
      GETARG_A(fp->code[0]) == 1 && GETARG_B(fp->code[0]) == 0 &&
      (GET_OPCODE(fp->code[1]) == OP_MMBINI ||
       GET_OPCODE(fp->code[1]) == OP_MMBIN) &&
      GET_OPCODE(fp->code[2]) == OP_MULK &&
      GETARG_A(fp->code[2]) == 1 && GETARG_B(fp->code[2]) == 1 &&
      ttisinteger(&fp->k[GETARG_C(fp->code[2])]) &&
      GET_OPCODE(fp->code[3]) == OP_MMBINK &&
      GET_OPCODE(fp->code[4]) == OP_RETURN1 &&
      GETARG_A(fp->code[4]) == 1 &&
      GET_OPCODE(fp->code[5]) == OP_RETURN0) {
    *mul = ivalue(&fp->k[GETARG_C(fp->code[2])]);
    *add = jit_wrap_mul(GETARG_sC(fp->code[0]), *mul);
    return 1;
  }
  return 0;
}


static lua_Integer leaf_single_apply (int op, lua_Integer v, lua_Integer imm) {
  switch (op) {
    case 1: return jit_wrap_mul(v, imm);
    case 2: return jit_wrap_add(v, imm);
    case 3: return jit_wrap_mul(v, v);
    default: return jit_wrap_add(v, v);
  }
}


static lua_Integer jit_idiv (lua_Integer a, lua_Integer b, int wantmod) {
  lua_Integer q;
  if (b == 0)
    return 0;
  if (b == -1) {
    q = (wantmod) ? 0 : l_castU2S(0u - l_castS2U(a));
    return q;
  }
  q = a / b;
  if ((a ^ b) < 0 && a % b != 0)
    q--;
  return wantmod ? (lua_Integer)(a - q * b) : q;
}


static int leaf_eval (Proto *fp, lua_Integer arg, lua_Integer arg2,
                      lua_Integer arg3, lua_Integer arg4, int nargs,
                      lua_Integer *out) {
  lua_Integer r[32];
  lu_byte kind[32];
  int pc, steps;
  if (fp->numparams != nargs || fp->sizeupvalues != 0 ||
      fp->maxstacksize > 32 || fp->sizecode > 32)
    return 0;
  memset(kind, 0, sizeof(kind));
  r[0] = arg;
  kind[0] = 1;
  if (nargs >= 2) {
    r[1] = arg2;
    kind[1] = 1;
  }
  if (nargs >= 3) {
    r[2] = arg3;
    kind[2] = 1;
  }
  if (nargs == 4) {
    r[3] = arg4;
    kind[3] = 1;
  }
  for (pc = 0, steps = 0; pc < fp->sizecode; pc++, steps++) {
    Instruction ins = fp->code[pc];
    OpCode op = GET_OPCODE(ins);
    int a = GETARG_A(ins);
    int b = GETARG_B(ins);
    int c = GETARG_C(ins);
    int cond;
    if (steps > 64)
      return 0;
    switch (op) {
      case OP_MOVE:
        if (a >= fp->maxstacksize || b >= fp->maxstacksize ||
            kind[b] == 0) return 0;
        r[a] = r[b];
        kind[a] = kind[b];
        break;
      case OP_LOADI:
        if (a >= fp->maxstacksize) return 0;
        r[a] = GETARG_sBx(ins);
        kind[a] = 1;
        break;
      case OP_LOADK:
        if (a >= fp->maxstacksize ||
            GETARG_Bx(ins) >= fp->sizek ||
            !ttisinteger(&fp->k[GETARG_Bx(ins)]))
          return 0;
        r[a] = ivalue(&fp->k[GETARG_Bx(ins)]);
        kind[a] = 1;
        break;
      case OP_LOADFALSE:
      case OP_LOADTRUE:
        if (a >= fp->maxstacksize) return 0;
        r[a] = (op == OP_LOADTRUE);
        kind[a] = 2;
        break;
      case OP_LOADNIL:
        if (a + b >= fp->maxstacksize) return 0;
        for (c = 0; c <= b; c++)
          kind[a + c] = 0;
        break;
      case OP_NOT:
        if (a >= fp->maxstacksize || b >= fp->maxstacksize ||
            kind[b] == 0) return 0;
        cond = (kind[b] == 2) ? (r[b] == 0) : 0;
        r[a] = cond;
        kind[a] = 2;
        break;
      case OP_ADDI:
        if (a >= fp->maxstacksize || b >= fp->maxstacksize ||
            kind[b] != 1) return 0;
        r[a] = jit_wrap_add(r[b], GETARG_sC(ins));
        kind[a] = 1;
        break;
      case OP_ADD:
      case OP_SUB:
      case OP_MUL:
      case OP_MOD:
      case OP_IDIV:
        if (a >= fp->maxstacksize || b >= fp->maxstacksize ||
            c >= fp->maxstacksize || kind[b] != 1 || kind[c] != 1)
          return 0;
        if (op == OP_ADD) r[a] = jit_wrap_add(r[b], r[c]);
        else if (op == OP_SUB)
          r[a] = l_castU2S(l_castS2U(r[b]) - l_castS2U(r[c]));
        else if (op == OP_MUL) r[a] = jit_wrap_mul(r[b], r[c]);
        else if (op == OP_MOD) r[a] = jit_idiv(r[b], r[c], 1);
        else r[a] = jit_idiv(r[b], r[c], 0);
        kind[a] = 1;
        break;
      case OP_ADDK:
      case OP_SUBK:
      case OP_MULK:
      case OP_MODK:
      case OP_IDIVK: {
        TValue *k;
        if (a >= fp->maxstacksize || b >= fp->maxstacksize ||
            kind[b] != 1 || c >= fp->sizek)
          return 0;
        k = &fp->k[c];
        if (!ttisinteger(k)) return 0;
        if (op == OP_ADDK) r[a] = jit_wrap_add(r[b], ivalue(k));
        else if (op == OP_SUBK)
          r[a] = jit_wrap_add(r[b], -ivalue(k));
        else if (op == OP_MULK) r[a] = jit_wrap_mul(r[b], ivalue(k));
        else if (op == OP_MODK) r[a] = jit_idiv(r[b], ivalue(k), 1);
        else r[a] = jit_idiv(r[b], ivalue(k), 0);
        kind[a] = 1;
        break;
      }
      case OP_UNM:
        if (a >= fp->maxstacksize || b >= fp->maxstacksize ||
            kind[b] != 1) return 0;
        r[a] = (lua_Integer)(0u - l_castS2U(r[b]));
        kind[a] = 1;
        break;
      case OP_EQ:
      case OP_LT:
      case OP_LE:
        if (a >= fp->maxstacksize || b >= fp->maxstacksize ||
            kind[a] == 0 || kind[a] != kind[b])
          return 0;
        if (kind[a] == 1)
          cond = (op == OP_EQ) ? (r[a] == r[b])
                 : (op == OP_LT) ? (r[a] < r[b]) : (r[a] <= r[b]);
        else if (op == OP_EQ)
          cond = (r[a] == r[b]);
        else
          return 0;
        if (cond != GETARG_k(ins)) pc++;
        break;
      case OP_EQK: {
        TValue *k;
        if (a >= fp->maxstacksize || b >= fp->sizek || kind[a] != 1)
          return 0;
        k = &fp->k[b];
        if (!ttisinteger(k)) return 0;
        cond = (r[a] == ivalue(k));
        if (cond != GETARG_k(ins)) pc++;
        break;
      }
      case OP_EQI:
      case OP_LTI:
      case OP_LEI:
      case OP_GTI:
      case OP_GEI:
        if (a >= fp->maxstacksize || kind[a] != 1)
          return 0;
        b = GETARG_sB(ins);
        cond = (op == OP_EQI) ? (r[a] == b)
               : (op == OP_LTI) ? (r[a] < b)
               : (op == OP_LEI) ? (r[a] <= b)
               : (op == OP_GTI) ? (r[a] > b) : (r[a] >= b);
        if (cond != GETARG_k(ins)) pc++;
        break;
      case OP_TEST:
        if (a >= fp->maxstacksize || kind[a] == 0) return 0;
        cond = (kind[a] == 2) ? (r[a] != 0) : 1;
        if (cond != GETARG_k(ins)) pc++;
        break;
      case OP_TESTSET:
        if (a >= fp->maxstacksize || b >= fp->maxstacksize ||
            kind[b] == 0) return 0;
        cond = (kind[b] == 2) ? (r[b] != 0) : 1;
        if ((!cond) == GETARG_k(ins))
          pc++;
        else {
          r[a] = r[b];
          kind[a] = kind[b];
        }
        break;
      case OP_JMP:
        pc += GETARG_sJ(ins);
        if (pc < -1 || pc >= fp->sizecode) return 0;
        break;
      case OP_MMBIN:
      case OP_MMBINI:
      case OP_MMBINK:
        break;
      case OP_RETURN1:
        if (a >= fp->maxstacksize || kind[a] != 1) return 0;
        *out = r[a];
        return 1;
      case OP_RETURN0:
        return 0;
      default:
        return 0;
    }
  }
  (void)steps;
  return 0;
}


static int leaf3_plan (Proto *fp, int *op) {
  if (fp->numparams != 3 || fp->sizeupvalues != 0 ||
      fp->sizecode < 6 || fp->maxstacksize < 4)
    return 0;
  if (GET_OPCODE(fp->code[0]) == OP_MUL &&
      GETARG_A(fp->code[0]) == 3 && GETARG_B(fp->code[0]) == 0 &&
      GETARG_C(fp->code[0]) == 1 &&
      GET_OPCODE(fp->code[1]) == OP_MMBIN) {
    if (GET_OPCODE(fp->code[2]) == OP_ADD &&
        GETARG_A(fp->code[2]) == 3 && GETARG_B(fp->code[2]) == 3 &&
        GETARG_C(fp->code[2]) == 2 &&
        GET_OPCODE(fp->code[3]) == OP_MMBIN &&
        GET_OPCODE(fp->code[4]) == OP_RETURN1 &&
        GETARG_A(fp->code[4]) == 3 &&
        GET_OPCODE(fp->code[5]) == OP_RETURN0) {
      *op = 1;  /* a*b + c */
      return 1;
    }
    if (GET_OPCODE(fp->code[2]) == OP_MUL &&
        GETARG_A(fp->code[2]) == 3 && GETARG_B(fp->code[2]) == 3 &&
        GETARG_C(fp->code[2]) == 2 &&
        GET_OPCODE(fp->code[3]) == OP_MMBIN &&
        GET_OPCODE(fp->code[4]) == OP_RETURN1 &&
        GETARG_A(fp->code[4]) == 3 &&
        GET_OPCODE(fp->code[5]) == OP_RETURN0) {
      *op = 2;  /* a*b*c */
      return 1;
    }
  }
  if ((GET_OPCODE(fp->code[0]) == OP_ADD ||
       GET_OPCODE(fp->code[0]) == OP_SUB) &&
      GETARG_A(fp->code[0]) == 3 && GETARG_B(fp->code[0]) == 0 &&
      GETARG_C(fp->code[0]) == 1 &&
      GET_OPCODE(fp->code[1]) == OP_MMBIN &&
      GET_OPCODE(fp->code[2]) == GET_OPCODE(fp->code[0]) &&
      GETARG_A(fp->code[2]) == 3 && GETARG_B(fp->code[2]) == 3 &&
      GETARG_C(fp->code[2]) == 2 &&
      GET_OPCODE(fp->code[3]) == OP_MMBIN &&
      GET_OPCODE(fp->code[4]) == OP_RETURN1 &&
      GETARG_A(fp->code[4]) == 3 &&
      GET_OPCODE(fp->code[5]) == OP_RETURN0) {
    *op = (GET_OPCODE(fp->code[0]) == OP_ADD) ? 3 : 4;
    return 1;
  }
  return 0;
}


static lua_Integer leaf3_apply (int op, lua_Integer a, lua_Integer b,
                                lua_Integer c) {
  switch (op) {
    case 1: return jit_wrap_add(jit_wrap_mul(a, b), c);
    case 2: return jit_wrap_mul(jit_wrap_mul(a, b), c);
    case 3: return jit_wrap_add(jit_wrap_add(a, b), c);
    default:
      return l_castU2S(l_castS2U(l_castU2S(l_castS2U(a) - l_castS2U(b))) -
                       l_castS2U(c));
  }
}


static int leaf2_plan (Proto *fp, int *op) {
  if (fp->numparams != 2 || fp->sizeupvalues != 0 ||
      fp->sizecode < 5 || fp->maxstacksize < 3)
    return 0;
  if (GET_OPCODE(fp->code[0]) == OP_MUL &&
      GETARG_A(fp->code[0]) == 2 && GETARG_B(fp->code[0]) == 0 &&
      GETARG_C(fp->code[0]) == 1 && GET_OPCODE(fp->code[1]) == OP_MMBIN &&
      GET_OPCODE(fp->code[2]) == OP_ADD &&
      GETARG_A(fp->code[2]) == 2 && GETARG_B(fp->code[2]) == 2 &&
      GETARG_C(fp->code[2]) == 1 && GET_OPCODE(fp->code[3]) == OP_MMBIN &&
      GET_OPCODE(fp->code[4]) == OP_RETURN1 &&
      GETARG_A(fp->code[4]) == 2) {
    *op = 1;  /* a*b + b */
    return 1;
  }
  if (fp->sizecode >= 4 &&
      (GET_OPCODE(fp->code[0]) == OP_MUL ||
       GET_OPCODE(fp->code[0]) == OP_ADD ||
       GET_OPCODE(fp->code[0]) == OP_SUB) &&
      GETARG_A(fp->code[0]) == 2 && GETARG_B(fp->code[0]) == 0 &&
      GETARG_C(fp->code[0]) == 1 &&
      GET_OPCODE(fp->code[1]) == OP_MMBIN &&
      GET_OPCODE(fp->code[2]) == OP_RETURN1 &&
      GETARG_A(fp->code[2]) == 2 &&
      GET_OPCODE(fp->code[3]) == OP_RETURN0) {
    *op = (GET_OPCODE(fp->code[0]) == OP_MUL) ? 2
          : (GET_OPCODE(fp->code[0]) == OP_ADD) ? 3 : 4;
    return 1;
  }
  return 0;
}


static lua_Integer leaf2_apply (int op, lua_Integer a, lua_Integer b) {
  switch (op) {
    case 1: return jit_wrap_add(jit_wrap_mul(a, b), b);
    case 2: return jit_wrap_mul(a, b);
    case 3: return jit_wrap_add(a, b);
    default: return l_castU2S(l_castS2U(a) - l_castS2U(b));
  }
}


static int leaf4_call_intrinsic (lua_State *L, TValue *base,
                                int for_a, int func_slot, int acc_slot,
                                int arg2_slot, int arg3_slot, int arg4_slot) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_slot];
  TValue *fv = &base[func_slot];
  TValue *a2 = &base[arg2_slot];
  TValue *a3 = &base[arg3_slot];
  TValue *a4 = &base[arg4_slot];
  LClosure *cl;
  lua_Integer count, step, idx, last, k, arg2, arg3, arg4;
  lua_Unsigned sum;
  (void)L;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      !ttisinteger(at) || !ttisinteger(a2) || !ttisinteger(a3) ||
      !ttisinteger(a4) || !ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  arg2 = ivalue(a2); arg3 = ivalue(a3); arg4 = ivalue(a4);
  count = ivalue(ct); step = ivalue(st); idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  {
    lua_Integer dummy;
    if (!leaf_eval(cl->p, idx, arg2, arg3, arg4, 4, &dummy))
      return 0;
  }
  last = idx + count;
  sum = l_castS2U(ivalue(at));
  for (k = 0; k <= count; k++) {
    lua_Integer v;
    if (!leaf_eval(cl->p, idx + k, arg2, arg3, arg4, 4, &v))
      return 0;
    sum += l_castS2U(v);
  }
  chgivalue(at, l_castU2S(sum));
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int leaf3_call_intrinsic (lua_State *L, TValue *base,
                                int for_a, int func_slot, int acc_slot,
                                int arg2_slot, int arg3_slot) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_slot];
  TValue *fv = &base[func_slot];
  TValue *a2 = &base[arg2_slot];
  TValue *a3 = &base[arg3_slot];
  LClosure *cl;
  lua_Integer count, step, idx, last, k, arg2, arg3;
  lua_Unsigned sum;
  int op;
  (void)L;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      !ttisinteger(at) || !ttisinteger(a2) || !ttisinteger(a3) ||
      !ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  arg2 = ivalue(a2);
  arg3 = ivalue(a3);
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  if (!leaf3_plan(cl->p, &op)) {
    lua_Integer dummy;
    if (!leaf_eval(cl->p, idx, arg2, arg3, 0, 3, &dummy))
      return 0;
    op = 0;
  }
  last = idx + count;
  sum = l_castS2U(ivalue(at));
  for (k = 0; k <= count; k++) {
    if (op == 0) {
      lua_Integer v;
      if (!leaf_eval(cl->p, idx + k, arg2, arg3, 0, 3, &v))
        return 0;
      sum += l_castS2U(v);
    }
    else
      sum += l_castS2U(leaf3_apply(op, idx + k, arg2, arg3));
  }
  chgivalue(at, l_castU2S(sum));
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int leaf2_call_intrinsic (lua_State *L, TValue *base,
                                int for_a, int func_slot, int acc_slot,
                                int arg2_slot) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_slot];
  TValue *fv = &base[func_slot];
  TValue *av = &base[arg2_slot];
  LClosure *cl;
  lua_Integer count, step, idx, last, k, arg2;
  lua_Unsigned sum;
  int op;
  (void)L;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      !ttisinteger(at) || !ttisinteger(av) || !ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  arg2 = ivalue(av);
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (!leaf2_plan(cl->p, &op)) {
    lua_Integer dummy;
    if (!leaf_eval(cl->p, idx, arg2, 0, 0, 2, &dummy))
      return 0;
    op = 0;  /* generic two-parameter evaluator */
  }
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  last = idx + count;
  sum = l_castS2U(ivalue(at));
  for (k = 0; k <= count; k++) {
    if (op == 0) {
      lua_Integer v;
      if (!leaf_eval(cl->p, idx + k, arg2, 0, 0, 2, &v))
        return 0;
      sum += l_castS2U(v);
    }
    else
      sum += l_castS2U(leaf2_apply(op, idx + k, arg2));
  }
  chgivalue(at, l_castU2S(sum));
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int leaf_call_intrinsic (lua_State *L, TValue *base,
                               int for_a, int func_slot, int acc_slot) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_slot];
  TValue *fv = &base[func_slot];
  LClosure *cl;
  lua_Integer count, step, idx, last, k, val;
  lua_Unsigned sum;
  int sop = 0;
  lua_Integer simm = 0;
  lua_Integer amul = 0, aadd = 0;
  int affine = 0;
  int bcmp = 0, bop1 = 0, bop2 = 0;
  lua_Integer bc = 0, bimm1 = 0, bimm2 = 0;
  int branchy = 0;
  (void)L;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it) ||
      !ttisinteger(at) || !ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (count < 0 || step != 1 || idx <= 0 || count > (1 << 22))
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  branchy = leaf_branch_plan(cl->p, &bcmp, &bc, &bop1, &bimm1,
                             &bop2, &bimm2);
  if (branchy) {
    last = idx + count;
    sum = l_castS2U(ivalue(at));
    for (k = 0; k <= count; k++)
      sum += l_castS2U(leaf_branch_apply(bcmp, idx + k, bc,
                                         bop1, bimm1, bop2, bimm2));
  }
  else if ((affine = leaf_affine_plan(cl->p, &amul, &aadd))) {
    last = idx + count;
    sum = l_castS2U(ivalue(at));
    for (k = 0; k <= count; k++)
      sum += l_castS2U(jit_wrap_add(jit_wrap_mul(idx + k, amul), aadd));
  }
  else if (leaf_single_plan(cl->p, &sop, &simm)) {
    last = idx + count;
    sum = l_castS2U(ivalue(at));
    for (k = 0; k <= count; k++)
      sum += l_castS2U(leaf_single_apply(sop, idx + k, simm));
  }
  else {
    if (!leaf_eval(cl->p, idx, 0, 0, 0, 1, &val))
      return 0;
    last = idx + count;
    sum = l_castS2U(ivalue(at));
    for (k = 0; k <= count; k++) {
      if (!leaf_eval(cl->p, idx + k, 0, 0, 0, 1, &val))
        return 0;
      sum += l_castS2U(val);
    }
  }
  chgivalue(at, l_castU2S(sum));
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static Table *tint_keys_table (Proto *p, TValue *base, int slot) {
  TValue *kt;
  if (p->luar_tint_upval[slot] >= 0) {
    TValue *fv = &base[-1];
    LClosure *cl;
    int idx = p->luar_tint_upval[slot];
    if (!ttisLclosure(fv))
      return NULL;
    cl = clLvalue(fv);
    if (idx >= cl->p->sizeupvalues)
      return NULL;
    kt = cl->upvals[idx]->v.p;
  }
  else
    kt = &base[p->luar_tint_keys[slot]];
  if (!ttistable(kt))
    return NULL;
  return hvalue(kt);
}


static int ir_floatify (Proto *p, TValue *base, LuaR_TraceIR *ir);

static int entry_sig_matches (Proto *p, int slot, TValue *base) {
  int k;
  uint64_t h = 1469598103934665603ull;
  if (p->luar_ngrd[slot] == 0)
    return 0;
  for (k = 0; k < p->luar_ngrd[slot]; k++) {
    int reg = p->luar_grd_reg[slot][k];
    int ty = ttisinteger(&base[reg]) ? LUAR_T_INT
             : ttisfloat(&base[reg]) ? LUAR_T_FLOAT
             : ttistable(&base[reg]) ? LUAR_T_TABLE : 0;
    h = (h ^ (uint64_t)reg) * 1099511628211ull;
    h = (h ^ (uint64_t)ty) * 1099511628211ull;
  }
  return h == p->luar_sig[slot];
}


static int jit_ir_side (LuaR_JIT *J, Proto *p, int pc, void *base, int expc, lua_State *L) {
  int ns = p->maxstacksize;
  if (getenv("LUAR_NO_SIDE") && !p->luar_ir_float) return -1;
  LuaR_IRExitState snap;
  int sr, lastpc = expc;
  if (ns > LUAR_IR_MAX_SLOTS) ns = LUAR_IR_MAX_SLOTS;
  if (p->luar_side_code == NULL && !p->luar_ir_side_done) {
    LuaR_TraceIR ir;
    int link;
    p->luar_ir_side_done = 1;
    if (expc < 0 || expc > pc ||
        !luar_trace_record_loop(p, expc, pc, (TValue *)base, &ir))
      return -1;
    {
      int floatized = p->luar_ir_float &&
                      ir_floatify(p, (TValue *)base, &ir);
      if (!floatized) {
        int gi;
        for (gi = 0; gi < ir.nins; gi++)
          if (ir.ins[gi].kind == LUAR_IR_GUARD &&
              ir.ins[gi].b == LUAR_IR_TAG_FLOAT)
            return -1;   /* mixed shape: do not emit integer side code */
      }
    }
    link = luar_trace_ir_link(&ir, 0, p->luar_ir_start);
    (void)luar_trace_ir_analyze(&ir, NULL, 0);
    if (link == 0 || (uint32_t)ir.depth > LUAR_IR_MAX_DEPTH) {
      J->ir_side_aborts++;
      return -1;
    }
    if (link == 2) J->ir_cycles++;
    J->ir_side_start = expc;
    J->ir_side_depth = ir.depth;
    if ((uint32_t)ir.depth > J->ir_max_depth)
      J->ir_max_depth = (uint32_t)ir.depth;
    J->ir_side_traces++;
    J->ir_side_ops += (uint32_t)ir.nins;
    ir.side = 1;
    if (J->ncode >= LUAR_JIT_MAXCODE)
      return -1;
    if (!luari_emit_trace_ir(&ir, &p->luar_side_code, &p->luar_side_size)) {
      p->luar_side_code = NULL;
      return -1;
    }
    p->luar_side_pc = pc;
    p->luar_side_start = expc;
    p->luar_side_epoch = J->epoch;
    J->code[J->ncode].code = p->luar_side_code;
    J->code[J->ncode].size = p->luar_side_size;
    J->ncode++;
    J->ir_side_compiled++;
  }
  if (p->luar_side_code == NULL || p->luar_side_epoch != J->epoch ||
      p->luar_side_pc != pc || p->luar_side_start != expc)
    return -1;
  if (!luar_trace_ir_snapshot(&snap, expc, (TValue *)base, ns))
    return -1;
  sr = ((LuaR_LoopFn)p->luar_side_code)(base, L);
  if (sr >= 0) {
    J->ir_side_executed++;
    return sr;
  }
  if (sr < -1) {
    int fpc = -sr - 2;
    int startpc = fpc;
    int level;
    lastpc = fpc;
    if (fpc == p->luar_side_start)
      return sr;   /* failed before any side mutation */
    for (level = 0; level < 3; level++) {
      LuaR_TraceIR cir;
      int csr;
      if (startpc <= p->luar_side_start ||
          (p->luar_side_block & (1ull << (startpc & 63))))
        break;
      if (p->luar_child_code[level] == NULL) {
        if (!luar_trace_record_loop(p, startpc, pc, (TValue *)base, &cir))
          break;
        if (!luar_trace_ir_link(&cir, level + 1, p->luar_ir_start))
          break;
        cir.side = 1;
        if (J->ncode >= LUAR_JIT_MAXCODE)
          break;
        if (!luari_emit_trace_ir(&cir, &p->luar_child_code[level],
                                 &p->luar_child_size[level]))
          break;
        p->luar_child_start[level] = startpc;
        p->luar_child_pc[level] = pc;
        p->luar_child_epoch[level] = J->epoch;
        J->code[J->ncode].code = p->luar_child_code[level];
        J->code[J->ncode].size = p->luar_child_size[level];
        J->ncode++;
        J->ir_side_compiled++;
        J->ir_side_traces++;
        J->ir_side_ops += (uint32_t)cir.nins;
        J->ir_side_start = startpc;
        J->ir_side_depth = cir.depth;
        if ((uint32_t)cir.depth > J->ir_max_depth)
          J->ir_max_depth = (uint32_t)cir.depth;
      }
      if (p->luar_child_code[level] == NULL ||
          p->luar_child_epoch[level] != J->epoch ||
          p->luar_child_pc[level] != pc ||
          p->luar_child_start[level] != startpc)
        break;
      csr = ((LuaR_LoopFn)p->luar_child_code[level])(base, L);
      if (csr == 2) {
        J->ir_yields++;
        return 0;
      }
      if (csr >= 0) {
        J->ir_side_executed++;
        return csr;
      }
      if (csr < -1) {
        fpc = -csr - 2;
        if (fpc > startpc) {
          startpc = fpc;
          lastpc = fpc;
          continue;
        }
      }
      p->luar_side_block |= (1ull << (startpc & 63));
      break;
    }
  }
  if (sr < -1)
    return -(lastpc + 2);
  luar_trace_ir_restore(&snap, (TValue *)base);
  return -1;
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
  if (J->enable_ir && p->luar_ir_code != NULL) {
    if (p->luar_ir_epoch != J->epoch) {
      p->luar_ir_code = NULL;
      p->luar_ir_size = 0;
      p->luar_ir_partial = 0;
      p->luar_ir_call_pc = -1;
    }
    else if (p->luar_ir_pc == pc && p->luar_ir_base == expected_base) {
      int irr = ((LuaR_LoopFn)p->luar_ir_code)(base, L);
      if (irr == 2) {
        J->ir_yields++;
        return 0;
      }
      if (irr > 0) {
        J->ir_executed++;
        return 1;
      }
      if (irr < -1) {
        int expc = -irr - 2;
        if (p->luar_ir_partial && expc == p->luar_ir_call_pc) {
          J->ir_call_exits++;
          return irr;
        }
        {
        int sr = jit_ir_side(J, p, pc, base, expc, L);
        if (sr >= 0) {
          J->ir_executed++;
          return sr;
        }
        if (sr < -1)
          return sr;
        return -(expc + 2);
        }
      }
    }
  }
  for (i = 0; i < 2; i++) {
    if (p->luar_tint[i] != 0 && p->luar_tint_pc[i] == pc &&
        p->luar_tint_base[i] == expected_base) {
      int r;
      if (p->luar_tint_epoch[i] != J->epoch) {
        p->luar_tint[i] = 0;
        continue;
      }
      switch (p->luar_tint[i]) {
        case 1:
          r = table_fill_intrinsic(L, (TValue *)base, expected_base,
                                   p->luar_tint_table[i]);
          break;
        case 2:
          r = table_sum_intrinsic(L, (TValue *)base, expected_base,
                                  p->luar_tint_table[i],
                                  p->luar_tint_acc[i]);
          break;
        case 3: {
          Table *keys = tint_keys_table(p, (TValue *)base, i);
          r = (keys == NULL) ? 0
              : keyed_fill_intrinsic(L, (TValue *)base, expected_base,
                                     p->luar_tint_table[i], keys);
          break;
        }
        case 9:
          r = leaf4_call_intrinsic(L, (TValue *)base, expected_base,
                                  p->luar_tint_table[i],
                                  p->luar_tint_acc[i],
                                  p->luar_tint_keys[i],
                                  p->luar_tint_upval[i],
                                  p->luar_tint_arg4[i]);
          break;
        case 8:
          r = leaf3_call_intrinsic(L, (TValue *)base, expected_base,
                                  p->luar_tint_table[i],
                                  p->luar_tint_acc[i],
                                  p->luar_tint_keys[i],
                                  p->luar_tint_upval[i]);
          break;
        case 7:
          r = leaf2_call_intrinsic(L, (TValue *)base, expected_base,
                                  p->luar_tint_table[i],
                                  p->luar_tint_acc[i],
                                  p->luar_tint_keys[i]);
          break;
        case 6:
          r = leaf_call_intrinsic(L, (TValue *)base, expected_base,
                                  p->luar_tint_table[i],
                                  p->luar_tint_acc[i]);
          break;
        case 5: {
          TValue *tt;
          if (p->luar_tint_upval[i] >= 0) {
            TValue *fv = &((TValue *)base)[-1];
            LClosure *cl;
            if (!ttisLclosure(fv))
              r = 0;
            else {
              cl = clLvalue(fv);
              if (p->luar_tint_upval[i] >= cl->p->sizeupvalues)
                r = 0;
              else {
                tt = cl->upvals[p->luar_tint_upval[i]]->v.p;
                r = method_add_intrinsic(L, p, (TValue *)base, expected_base,
                                         tt, p->luar_tint_keys[i]);
              }
            }
          }
          else
            r = method_add_intrinsic(L, p, (TValue *)base, expected_base,
                                     &((TValue *)base)[p->luar_tint_table[i]],
                                     p->luar_tint_keys[i]);
          break;
        }
        default: {
          Table *keys = tint_keys_table(p, (TValue *)base, i);
          r = (keys == NULL) ? 0
              : keyed_sum_intrinsic(L, (TValue *)base, expected_base,
                                    p->luar_tint_table[i], keys,
                                    p->luar_tint_acc[i]);
          break;
        }
      }
      if (r == 0)
        J->exits++;
      return r;
    }
  }
  {
    int any = 0;
    int order[2], nord = 0, first, pref = -1;
    for (i = 0; i < 2; i++) {
      if (p->luar_code[i] != NULL && p->luar_code_pc[i] == pc &&
          p->luar_code_base[i] == expected_base &&
          p->luar_code_epoch[i] == J->epoch &&
          entry_sig_matches(p, i, (TValue *)base)) {
        pref = i;
        break;
      }
    }
    first = (pref >= 0) ? pref : 0;
    order[nord++] = first;
    if (first == 0)
      order[nord++] = 1;
    else
      order[nord++] = 0;
    if (pref >= 0)
      J->sig_hits++;
    for (first = 0; first < nord; first++) {
      int r;
      i = order[first];
      if (p->luar_code[i] == NULL || p->luar_code_pc[i] != pc ||
          p->luar_code_base[i] != expected_base)
        continue;
      if (p->luar_code_epoch[i] != J->epoch) {
        p->luar_code[i] = NULL;   /* flushed: mapping was released */
        continue;
      }
      if (p->luar_meta[i]) {
        TValue *tab;
        int64_t mult;
        if (p->luar_meta_upval[i] >= 0) {
          TValue *fv = &((TValue *)base)[-1];
          LClosure *cl;
          if (!ttisLclosure(fv))
            return 0;
          cl = clLvalue(fv);
          if (p->luar_meta_upval[i] >= cl->p->sizeupvalues)
            return 0;
          tab = cl->upvals[p->luar_meta_upval[i]]->v.p;
        }
        else
          tab = &((TValue *)base)[p->luar_meta_table[i]];
        if (!ttisinteger(&((TValue *)base)[p->luar_meta_acc[i]]) ||
            !meta_multiplier(L, tab, &mult) ||
            mult != p->luar_meta_mult[i])
          return 0;
      }
      if (p->luar_fc_closure[i] != NULL) {
        TValue *fv = &((TValue *)base)[p->luar_fc_func[i]];
        LClosure *cl;
        UpVal *uv;
        if (!ttisLclosure(fv))
          return 0;
        cl = clLvalue(fv);
        if ((void *)cl->p != p->luar_fc_closure[i])
          return 0;
        uv = cl->upvals[0];
        if (uv->v.p != &((TValue *)base)[p->luar_fc_upval[i]] ||
            !ttisinteger(uv->v.p))
          return 0;
      }
      any = 1;
      r = ((LuaR_LoopFn)p->luar_code[i])(base, L);
      if (r != 0) {
        if (r < -1) {
          int expc = -r - 2;
          J->last_exit_pc = expc;
          if (!p->luar_ir_side_done) {
            LuaR_TraceIR ir;
            p->luar_ir_side_done = 1;
            if (expc >= 0 && expc <= pc &&
                luar_trace_record_loop(p, expc, pc, (TValue *)base, &ir)) {
              int link = luar_trace_ir_link(&ir, 0,
                                            p->luar_code_start[i]);
              (void)luar_trace_ir_analyze(&ir, NULL, 0);
              if (link == 0) {
                J->ir_side_aborts++;
              }
              else {
                if (link == 2)
                  J->ir_cycles++;
                if ((uint32_t)ir.depth > J->ir_max_depth)
                  J->ir_max_depth = (uint32_t)ir.depth;
                J->ir_side_traces++;
                J->ir_side_ops += (uint32_t)ir.nins;
                {
                  int has_tset = 0, k;
                  for (k = 0; k < ir.nins; k++)
                    if (ir.ins[k].kind == LUAR_IR_TSET) has_tset = 1;
                  if (!has_tset && p->luar_side_code == NULL &&
                      J->ncode < LUAR_JIT_MAXCODE) {
                    void *sc = NULL;
                    size_t ss = 0;
                    ir.side = 1;
                    if (luari_emit_trace_ir(&ir, &sc, &ss)) {
                      p->luar_side_code = sc;
                      p->luar_side_size = ss;
                      p->luar_side_pc = pc;
                      p->luar_side_start = expc;
                      p->luar_side_epoch = J->epoch;
                      J->code[J->ncode].code = sc;
                      J->code[J->ncode].size = ss;
                      J->ncode++;
                      J->ir_side_compiled++;
                    }
                  }
                }
              }
            }
            else
              J->ir_side_aborts++;
          }
          if (p->luar_side_code != NULL &&
              p->luar_side_epoch == J->epoch &&
              p->luar_side_pc == pc && p->luar_side_start == expc) {
            LuaR_IRExitState snap;
            int ns = p->maxstacksize;
            if (ns > LUAR_IR_MAX_SLOTS) ns = LUAR_IR_MAX_SLOTS;
            if (luar_trace_ir_snapshot(&snap, expc, (TValue *)base, ns)) {
              int sr = ((LuaR_LoopFn)p->luar_side_code)(base, L);
              if (sr >= 0) {
                J->ir_side_executed++;
                return sr;
              }
              luar_trace_ir_restore(&snap, (TValue *)base);
            }
          }
        }
        return r;
      }
      J->exits++;
      p->luar_code_exits[i]++;
    }
    if (any && !p->luar_variant_tried &&
        (p->luar_code_exits[0] >= J->hotexit ||
         p->luar_code_exits[1] >= J->hotexit) &&
        (p->luar_code[0] == NULL || p->luar_code[1] == NULL) &&
        p->luar_tint[0] == 0 && p->luar_tint[1] == 0) {
      p->luar_variant_tried = 1;
      {
        int before = (p->luar_code[0] != NULL) + (p->luar_code[1] != NULL);
        luari_jit_hotloop(L, p, &p->code[pc], base);
        if ((p->luar_code[0] != NULL) + (p->luar_code[1] != NULL) > before)
          J->side_traces++;
      }
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

static int ir_floatify (Proto *p, TValue *base, LuaR_TraceIR *ir) {
  int i, fslot = -1, sawtable = 0, sawfloat = 0;
  for (i = 0; i < ir->nins; i++) {
    LuaR_IRIns *op = &ir->ins[i];
    if (op->kind == LUAR_IR_GUARD) {
      if (op->b != LUAR_IR_TAG_INT && op->b != LUAR_IR_TAG_FLOAT &&
          op->b != LUAR_IR_TAG_TABLE)
        return 0;
      continue;
    }
    if (op->kind == LUAR_IR_CGETF) {
      fslot = op->a;
      sawfloat = 1;
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TGET && !sawtable) {
      Instruction ex;
      int tb, tk;
      double fv;
      if (op->pc < 0 || op->pc >= p->sizecode)
        return 0;
      ex = p->code[op->pc];
      if (GET_OPCODE(ex) != OP_GETTABLE)
        return 0;
      tb = GETARG_B(ex);
      tk = GETARG_C(ex);
      if (!ttisinteger(&base[tk]))
        return 0;
      if (luar_trace_tgetf_generic(&base[tb], ivalue(&base[tk]), &fv)) {
        op->kind = LUAR_IR_TGETF;
        fslot = op->a;
        sawfloat = 1;
      }
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_MULK) {
      int is_float = (op->nval != (double)op->ival);
      if ((sawfloat && op->b == fslot) || is_float) {
        op->kind = LUAR_IR_FMULK;
        fslot = op->a;
        sawfloat = 1;
        continue;
      }
    }
    if (op->kind == LUAR_IR_CSETF && sawfloat && op->c == fslot) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_ADD && sawfloat &&
        (op->b == fslot || op->c == fslot)) {
      op->kind = LUAR_IR_FADD;
      fslot = op->a;
      continue;
    }
    if (op->kind == LUAR_IR_SUB && sawfloat &&
        (op->b == fslot || op->c == fslot)) {
      op->kind = LUAR_IR_FSUB;
      fslot = op->a;
      continue;
    }
    if (op->kind == LUAR_IR_DIVK && (sawfloat || op->nval != (double)op->ival)) {
      op->kind = LUAR_IR_FDIVK;
      fslot = op->a;
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_FORLOOP || op->kind == LUAR_IR_RET ||
        op->kind == LUAR_IR_EXIT || op->kind == LUAR_IR_CGUARD)
      continue;
    return 0;
  }
  return sawtable && sawfloat && fslot >= 0;
}


static int ir_integer_only (const LuaR_TraceIR *ir) {
  int i, tag[LUAR_IR_MAX_SLOTS];
  for (i = 0; i < LUAR_IR_MAX_SLOTS; i++)
    tag[i] = -1;
  for (i = 0; i < ir->nins; i++) {
    const LuaR_IRIns *in = &ir->ins[i];
    if (in->kind == LUAR_IR_CGETF)
      return 0;
    if (in->kind == LUAR_IR_GUARD) {
      if (in->b != LUAR_IR_TAG_INT && in->b != LUAR_IR_TAG_TABLE)
        return 0;
      tag[in->a] = (uint8_t)in->b;
    }
  }
  for (i = 0; i < ir->nins; i++) {
    const LuaR_IRIns *in = &ir->ins[i];
    if (in->kind == LUAR_IR_ADD || in->kind == LUAR_IR_SUB ||
        in->kind == LUAR_IR_MUL || in->kind == LUAR_IR_DIV) {
      if (tag[in->b] == LUAR_IR_TAG_TABLE ||
          tag[in->b] == LUAR_IR_TAG_OTHER ||
          tag[in->c] == LUAR_IR_TAG_TABLE ||
          tag[in->c] == LUAR_IR_TAG_OTHER)
        return 0;
    }
    if ((in->kind == LUAR_IR_ADDI ||
         (in->kind >= LUAR_IR_ADDK && in->kind <= LUAR_IR_IDIVK)) &&
        (tag[in->b] == LUAR_IR_TAG_TABLE ||
         tag[in->b] == LUAR_IR_TAG_OTHER))
      return 0;
    if (in->kind >= LUAR_IR_ADDK && in->kind <= LUAR_IR_IDIVK &&
        in->nval != (double)in->ival)
      return 0;
  }
  return 1;
}


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
  {
    LuaR_TraceIR ir;
    int callpc = -1, partial = 0;
    int recorded = luar_trace_record_loop(p, (int)target, (int)off,
                                          (TValue *)base, &ir);
    if (!recorded) {
      partial = luar_trace_record_call_prefix(p, (int)target, (int)off,
                                              (TValue *)base, &ir, &callpc);
      recorded = partial;
    }
    if (recorded) {
      J->ir_traces++;
      J->ir_ops += (uint32_t)ir.nins;
      int newcode = 0;
      {
        int intok = ir_integer_only(&ir);
        int fltok = (!intok && ir_floatify(p, (TValue *)base, &ir));
        if (J->enable_ir && p->luar_ir_code == NULL &&
            GET_OPCODE(ins) == OP_FORLOOP && (intok || fltok)) {
          if (luari_emit_trace_ir(&ir, &p->luar_ir_code,
                                  &p->luar_ir_size)) {
            p->luar_ir_float = fltok;
            newcode = 1;
          }
        }
      }
      if (newcode && J->enable_ir && p->luar_ir_code != NULL &&
          p->luar_ir_pc != (int)off &&
          J->ncode < LUAR_JIT_MAXCODE) {
        J->code[J->ncode].code = p->luar_ir_code;
        J->code[J->ncode].size = p->luar_ir_size;
        J->ncode++;
        J->ir_compiled++;
        J->ir_code_bytes += (uint32_t)p->luar_ir_size;
        p->luar_ir_pc = (int)off;
        p->luar_ir_start = (int)target;
        p->luar_ir_partial = partial;
        p->luar_ir_call_pc = callpc;
        p->luar_ir_table_slot = -1;
        p->luar_ir_key_slot = -1;
        p->luar_ir_elem_tag = 0;
        {
          int ii;
          for (ii = 0; ii < ir.nins; ii++) {
            LuaR_IRIns *o = &ir.ins[ii];
            if (o->kind == LUAR_IR_TGET || o->kind == LUAR_IR_TGETF) {
              p->luar_ir_table_slot = o->b;
              p->luar_ir_key_slot = o->c;
              p->luar_ir_elem_tag = (o->kind == LUAR_IR_TGETF) ?
                                    LUAR_IR_TAG_FLOAT : LUAR_IR_TAG_INT;
              break;
            }
          }
        }
        J->ir_root_start = (int)target;
        p->luar_ir_base = GETARG_A(ins);
        p->luar_ir_epoch = J->epoch;
      }
      else {
        void *ir_code = NULL;
        size_t ir_size = 0;
        if (luari_emit_trace_ir(&ir, &ir_code, &ir_size)) {
          J->ir_compiled++;
          J->ir_code_bytes += (uint32_t)ir_size;
          munmap(ir_code, ir_size);
        }
      }
      if (J->dump_ir) {
        fprintf(stderr, "-- j4 ir: %d ops, %d exits, depth %d\n",
                ir.nins, ir.nexits, ir.depth);
      }
    }
    else
      J->ir_aborts++;
  }
  if (J->enabled && !J->enable_ir) {
    int ok = try_compile(L, p, target, off, base);
    int full = (p->luar_code[0] != NULL || p->luar_tint[0] != 0) &&
               (p->luar_code[1] != NULL || p->luar_tint[1] != 0);
    if (!ok || full)
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
  /* Accept and keep LuaJIT-style option strings.  Luar's experimental
  ** table JIT is opt-in ("table=1") because it is not yet a performance
  ** win on the reference corpus.
  */
  LuaR_JIT *J = checkjit(L);
  int i;
  int n = lua_gettop(L);
  lua_newtable(L);
  for (i = 1; i <= n; i++) {
    const char *opt = luaL_checkstring(L, i);
    if (J != NULL && strncmp(opt, "ir=", 3) == 0)
      J->enable_ir = (opt[3] != '0');
    else if (J != NULL && strncmp(opt, "table=", 6) == 0) {
      J->enable_table = (opt[6] != '0');
      J->enable_tint = (opt[6] != '0');
    }
    else if (J != NULL && strncmp(opt, "tint=", 5) == 0)
      J->enable_tint = (opt[5] != '0');
    else if (J != NULL && strncmp(opt, "call=", 5) == 0)
      J->enable_partial = (opt[5] != '0');
    else if (J != NULL && strncmp(opt, "hotexit=", 8) == 0) {
      unsigned long v = strtoul(opt + 8, NULL, 10);
      if (v >= 1 && v <= 1000000)
        J->hotexit = (uint32_t)v;
    }
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
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->side_traces : 0);
  lua_setfield(L, -2, "side_traces");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->last_exit_pc : -1);
  lua_setfield(L, -2, "last_exit_pc");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->sig_hits : 0);
  lua_setfield(L, -2, "sig_hits");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_traces : 0);
  lua_setfield(L, -2, "ir_traces");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_ops : 0);
  lua_setfield(L, -2, "ir_ops");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_aborts : 0);
  lua_setfield(L, -2, "ir_aborts");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_traces : 0);
  lua_setfield(L, -2, "ir_side_traces");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_ops : 0);
  lua_setfield(L, -2, "ir_side_ops");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_aborts : 0);
  lua_setfield(L, -2, "ir_side_aborts");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_max_depth : 0);
  lua_setfield(L, -2, "ir_max_depth");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_cycles : 0);
  lua_setfield(L, -2, "ir_cycles");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_compiled : 0);
  lua_setfield(L, -2, "ir_compiled");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_code_bytes : 0);
  lua_setfield(L, -2, "ir_code_bytes");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_executed : 0);
  lua_setfield(L, -2, "ir_executed");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_compiled : 0);
  lua_setfield(L, -2, "ir_side_compiled");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_executed : 0);
  lua_setfield(L, -2, "ir_side_executed");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_call_exits : 0);
  lua_setfield(L, -2, "ir_call_exits");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_start : -1);
  lua_setfield(L, -2, "ir_side_start");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_root_start : -1);
  lua_setfield(L, -2, "ir_root_start");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_yields : 0);
  lua_setfield(L, -2, "ir_yields");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_depth : 0);
  lua_setfield(L, -2, "ir_side_depth");
  lua_pushboolean(L, luar_has_jit() && J != NULL && J->enabled);
  lua_setfield(L, -2, "status");
  return 1;
}


static int jit_trace_ir_selftest (lua_State *L) {
  lua_pushboolean(L, luar_trace_ir_selftest());
  return 1;
}


static int jit_trace_exec_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  TValue base[4];
  void *code = NULL;
  size_t size = 0;
  int ok = 0;
  LuaR_LoopFn fn;
  luar_trace_ir_init(&t, NULL, 0, 4, 0);
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_LOADI; in.a = 0; in.ival = 5;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_ADDI; in.a = 0; in.b = 0; in.ival = 7;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_FORLOOP; in.a = 1;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  if (!luar_trace_ir_verify(&t, NULL, 0)) goto done;
  if (!luari_emit_trace_ir(&t, &code, &size)) goto done;
  memset(base, 0, sizeof(base));
  fn = (LuaR_LoopFn)code;
  if (fn(base, L) != 1) goto done;
  if (!ttisinteger(&base[0]) || ivalue(&base[0]) != 12) goto done;
  ok = 1;
done:
  if (code != NULL)
    munmap(code, size);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_side_exec_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  TValue base[4];
  void *code = NULL;
  size_t size = 0;
  int ok = 0;
  LuaR_LoopFn fn;
  luar_trace_ir_init(&t, NULL, 17, 4, 1);
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_LOADI; in.a = 0; in.ival = 3;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_ADDI; in.a = 0; in.b = 0; in.ival = 4;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  if (!luar_trace_ir_verify(&t, NULL, 0)) goto done;
  if (!luari_emit_trace_ir(&t, &code, &size)) goto done;
  memset(base, 0, sizeof(base));
  fn = (LuaR_LoopFn)code;
  if (fn(base, L) != 1) goto done;
  if (!ttisinteger(&base[0]) || ivalue(&base[0]) != 7) goto done;
  ok = 1;
done:
  if (code != NULL)
    munmap(code, size);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_add_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  TValue base[2];
  void *code = NULL;
  size_t size = 0;
  int ok = 0;
  LuaR_LoopFn fn;
  luar_trace_ir_init(&t, NULL, 0, 2, 0);
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_LOADI; in.a = 0; in.ival = 10;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_LOADI; in.a = 1; in.ival = 5;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_ADD; in.a = 0; in.b = 0; in.c = 1;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  if (!luar_trace_ir_verify(&t, NULL, 0)) goto done;
  if (!luari_emit_trace_ir(&t, &code, &size)) goto done;
  memset(base, 0, sizeof(base));
  fn = (LuaR_LoopFn)code;
  if (fn(base, L) != 1) goto done;
  if (!ttisinteger(&base[0]) || ivalue(&base[0]) != 15) goto done;
  ok = 1;
done:
  if (code != NULL) munmap(code, size);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_snapshot_selftest (lua_State *L) {
  LuaR_IRExitState ex;
  TValue base[LUAR_IR_MAX_SLOTS];
  int i, ok = 1;
  memset(base, 0, sizeof(base));
  for (i = 0; i < LUAR_IR_MAX_SLOTS; i++) {
    if (i & 1) { setivalue(&base[i], i + 1); }
    else setfltvalue(&base[i], (double)i + 0.25);
  }
  if (!luar_trace_ir_snapshot(&ex, 7, base, LUAR_IR_MAX_SLOTS)) return 0;
  if (luar_trace_ir_snapshot(&ex, 7, base, LUAR_IR_MAX_SLOTS + 1)) return 0;
  for (i = 0; i < LUAR_IR_MAX_SLOTS; i++)
    setivalue(&base[i], 0);
  if (!luar_trace_ir_restore(&ex, base)) return 0;
  for (i = 0; i < LUAR_IR_MAX_SLOTS; i++) {
    if (i & 1) {
      if (!ttisinteger(&base[i]) || ivalue(&base[i]) != i + 1) ok = 0;
    } else if (!ttisfloat(&base[i]) ||
               fltvalue(&base[i]) != (double)i + 0.25) ok = 0;
  }
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_side_fail_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  LuaR_IRExitState snap;
  TValue base[3];
  void *code = NULL;
  size_t size = 0;
  int ok = 0;
  LuaR_LoopFn fn;
  extern int64_t luar_trace_tget_int (TValue *, lua_Integer, int64_t *);
  luar_trace_ir_init(&t, NULL, 5, 3, 1);
  t.side = 1;
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_TGET; in.a = 0; in.b = 1; in.c = 2; in.pc = 5;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  if (!luar_trace_ir_verify(&t, NULL, 0)) goto done;
  if (!luari_emit_trace_ir(&t, &code, &size)) goto done;
  memset(base, 0, sizeof(base));
  setivalue(&base[1], 7);       /* not a table: TGET must fail */
  setivalue(&base[2], 1);
  setivalue(&base[0], 99);
  if (!luar_trace_ir_snapshot(&snap, 5, base, 3)) goto done;
  fn = (LuaR_LoopFn)code;
  if (fn(base, L) != -7) goto done;
  setivalue(&base[0], 1234);
  if (!luar_trace_ir_restore(&snap, base)) goto done;
  if (ivalue(&base[0]) != 99) goto done;
  ok = 1;
done:
  if (code != NULL) munmap(code, size);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_loop_exec_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  TValue base[4];
  void *code = NULL;
  size_t size = 0;
  int ok = 0;
  LuaR_LoopFn fn;
  luar_trace_ir_init(&t, NULL, 0, 4, 0);
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_ADD; in.a = 0; in.b = 0; in.c = 3;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_FORLOOP; in.a = 1;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  if (!luar_trace_ir_verify(&t, NULL, 0)) goto done;
  if (!luari_emit_trace_ir(&t, &code, &size)) goto done;
  memset(base, 0, sizeof(base));
  setivalue(&base[0], 0);
  setivalue(&base[1], 3);
  setivalue(&base[2], 1);
  setivalue(&base[3], 0);
  fn = (LuaR_LoopFn)code;
  if (fn(base, L) != 1) goto done;
  if (!ttisinteger(&base[0]) || ivalue(&base[0]) != 6) goto done;
  ok = 1;
done:
  if (code != NULL) munmap(code, size);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_tget_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  TValue base[3];
  void *code = NULL;
  size_t size = 0;
  int ok = 0;
  LuaR_LoopFn fn;
  lua_createtable(L, 1, 0);
  lua_pushinteger(L, 42);
  lua_seti(L, -2, 1);
  luar_trace_ir_init(&t, NULL, 0, 3, 0);
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_GUARD; in.a = 1; in.b = LUAR_IR_TAG_TABLE;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_GUARD; in.a = 2; in.b = LUAR_IR_TAG_INT;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_TGET; in.a = 0; in.b = 1; in.c = 2;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  if (!luar_trace_ir_verify(&t, NULL, 0)) goto done;
  if (!luari_emit_trace_ir(&t, &code, &size)) goto done;
  memset(base, 0, sizeof(base));
  lua_pushvalue(L, -1);
  base[1] = *s2v(L->top.p - 1);
  lua_pop(L, 1);
  setivalue(&base[2], 1);
  fn = (LuaR_LoopFn)code;
  if (fn(base, L) != 1) goto done;
  if (!ttisinteger(&base[0]) || ivalue(&base[0]) != 42) goto done;
  ok = 1;
done:
  if (code != NULL) munmap(code, size);
  lua_pop(L, 1);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_side_link_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  TValue base[4];
  void *code = NULL;
  size_t size = 0;
  int ok = 0;
  LuaR_LoopFn fn;
  luar_trace_ir_init(&t, NULL, 5, 4, 1);
  t.side = 1;
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_ADD; in.a = 0; in.b = 0; in.c = 3; in.pc = 5;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_FORLOOP; in.a = 1; in.pc = 6;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_RET; in.pc = 7;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  if (!luar_trace_ir_verify(&t, NULL, 0)) goto done;
  if (!luari_emit_trace_ir(&t, &code, &size)) goto done;
  memset(base, 0, sizeof(base));
  setivalue(&base[0], 0);
  setivalue(&base[1], 3);
  setivalue(&base[2], 1);
  setivalue(&base[3], 0);
  fn = (LuaR_LoopFn)code;
  if (fn(base, L) != 0) goto done;
  if (ivalue(&base[1]) != 2 || ivalue(&base[3]) != 1) goto done;
  ok = 1;
done:
  if (code != NULL) munmap(code, size);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_hash_get_selftest (lua_State *L) {
  TValue tv;
  int64_t v = 0;
  int ok;
  lua_createtable(L, 0, 1);
  lua_pushinteger(L, 42);
  lua_seti(L, -2, 100000);
  lua_pushvalue(L, -1);
  tv = *s2v(L->top.p - 1);
  ok = (int)luar_trace_tget_generic(&tv, 100000, &v) && v == 42;
  lua_pop(L, 2);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_tget_generic_ir_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  TValue base[3];
  void *code = NULL;
  size_t size = 0;
  int ok = 0;
  LuaR_LoopFn fn;
  lua_createtable(L, 0, 1);
  lua_pushinteger(L, 42);
  lua_seti(L, -2, 100000);
  luar_trace_ir_init(&t, NULL, 0, 3, 1);
  t.side = 1;
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_TGET; in.a = 0; in.b = 1; in.c = 2;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) goto done;
  if (!luar_trace_ir_verify(&t, NULL, 0)) goto done;
  if (!luari_emit_trace_ir(&t, &code, &size)) goto done;
  memset(base, 0, sizeof(base));
  lua_pushvalue(L, -1);
  base[1] = *s2v(L->top.p - 1);
  lua_pop(L, 1);
  setivalue(&base[2], 100000);
  fn = (LuaR_LoopFn)code;
  if (fn(base, L) != 1) goto done;
  if (!ttisinteger(&base[0]) || ivalue(&base[0]) != 42) goto done;
  ok = 1;
done:
  if (code != NULL) munmap(code, size);
  lua_pop(L, 1);
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_link_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  int ok = 1;
  luar_trace_ir_init(&t, NULL, 5, 4, 0);
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_FORLOOP; in.a = 1;
  if (!luar_trace_ir_emit(&t, &in)) return 0;
  in.kind = LUAR_IR_RET;
  if (!luar_trace_ir_emit(&t, &in)) return 0;
  if (luar_trace_ir_link(&t, 0, 5) != 2 || t.depth != 1) ok = 0;
  t.startpc = 6;
  if (luar_trace_ir_link(&t, 3, 5) != 1 || t.depth != 4) ok = 0;
  if (luar_trace_ir_link(&t, 4, 5) != 0) ok = 0;
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_guard_selftest (lua_State *L) {
  LuaR_TraceIR t;
  LuaR_IRIns in;
  TValue base[2];
  int ok;
  luar_trace_ir_init(&t, NULL, 0, 2, 0);
  memset(&in, 0, sizeof(in));
  in.kind = LUAR_IR_GUARD; in.a = 0; in.b = LUAR_IR_TAG_INT;
  if (!luar_trace_ir_emit(&t, &in)) ok = 0;
  else {
    memset(base, 0, sizeof(base));
    setivalue(&base[0], 1);
    ok = luar_trace_ir_guards_match(&t, base);
    setfltvalue(&base[0], 1.5);
    if (luar_trace_ir_guards_match(&t, base))
      ok = 0;
  }
  lua_pushboolean(L, ok);
  return 1;
}


static int jit_trace_ir (lua_State *L) {
  lua_newtable(L);
  lua_pushinteger(L, LUAR_IR_VERSION);
  lua_setfield(L, -2, "version");
  lua_pushinteger(L, LUAR_IR_MAX_INS);
  lua_setfield(L, -2, "max_ops");
  lua_pushinteger(L, LUAR_IR_MAX_EXITS);
  lua_setfield(L, -2, "max_exits");
  lua_pushcfunction(L, jit_trace_ir_selftest);
  lua_setfield(L, -2, "selftest");
  lua_pushcfunction(L, jit_trace_exec_selftest);
  lua_setfield(L, -2, "exec_selftest");
  lua_pushcfunction(L, jit_trace_side_exec_selftest);
  lua_setfield(L, -2, "side_exec_selftest");
  lua_pushcfunction(L, jit_trace_guard_selftest);
  lua_setfield(L, -2, "guard_selftest");
  lua_pushcfunction(L, jit_trace_link_selftest);
  lua_setfield(L, -2, "link_selftest");
  lua_pushcfunction(L, jit_trace_tget_generic_ir_selftest);
  lua_setfield(L, -2, "tget_generic_ir_selftest");
  lua_pushcfunction(L, jit_trace_hash_get_selftest);
  lua_setfield(L, -2, "hash_get_selftest");
  lua_pushcfunction(L, jit_trace_side_link_selftest);
  lua_setfield(L, -2, "side_link_selftest");
  lua_pushcfunction(L, jit_trace_side_fail_selftest);
  lua_setfield(L, -2, "side_fail_selftest");
  lua_pushcfunction(L, jit_trace_snapshot_selftest);
  lua_setfield(L, -2, "snapshot_selftest");
  lua_pushcfunction(L, jit_trace_tget_selftest);
  lua_setfield(L, -2, "tget_selftest");
  lua_pushcfunction(L, jit_trace_loop_exec_selftest);
  lua_setfield(L, -2, "loop_exec_selftest");
  lua_pushcfunction(L, jit_trace_add_selftest);
  lua_setfield(L, -2, "add_selftest");
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
  {"trace_ir", jit_trace_ir},
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
  lua_pushliteral(L, "Luar " LUAR_VERSION);
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
