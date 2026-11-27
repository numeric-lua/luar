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
#include <math.h>
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
static TValue *pair_source_value (TValue *base, int reg, int upval);

/*
** Clang's -fsanitize=function (part of -fsanitize=undefined) checks an
** indirect call target for a type word stored just before its entry
** point.  JIT-compiled code has no such word and can start at a mapping
** boundary, so every function that enters generated code opts out of
** that one check; the remaining sanitizers still apply.
*/
#if defined(__clang__) && defined(__has_attribute)
#if __has_attribute(no_sanitize)
#define LUAR_NO_SANITIZE_FUNCTION \
  __attribute__((no_sanitize("function")))
#endif
#endif

#ifndef LUAR_NO_SANITIZE_FUNCTION
#define LUAR_NO_SANITIZE_FUNCTION
#endif


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
  J->tint_hits = 0;
  J->side_traces = 0;
  J->last_exit_pc = -1;
  J->sig_hits = 0;
  J->ir_executed = 0;
  J->ir_entry_checks = 0;
  J->ir_entry_pc_mismatch = 0;
  J->ir_entry_base_mismatch = 0;
  J->ir_entry_failures = 0;
  J->ir_last_entry_result = 0;
  J->ir_side_compiled = 0;
  J->ir_side_executed = 0;
  J->ir_call_exits = 0;
  J->ir_cont_compiled = 0;
  J->ir_cont_runs = 0;
  J->ir_install_rejects = 0;
  J->ir_alloc_tables = 0;
  J->ir_side_start = -1;
  J->ir_side_parent = -1;
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
                             int acc, int res) {
  int slot = -1, k;
  if (res == acc)
    return 0;
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
  p->luar_tint_suspend[slot] = 0;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = table;
  p->luar_tint_keys[slot] = keys;
  p->luar_tint_upval[slot] = upval;
  p->luar_tint_acc[slot] = acc;
  p->luar_tint_res[slot] = res;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


/* LUAR: `s = s + t[i]` where `t` is a closure upvalue.  The table is
** resolved from the current closure at each entry, so reassigning the
** upvalue is observed like the interpreter. */
static int install_upval_sum (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                              int for_a, int upval, int acc, int res) {
  int slot = -1, k;
  if (res == acc)
    return 0;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 10;
  p->luar_tint_suspend[slot] = 0;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = 0;
  p->luar_tint_keys[slot] = 0;
  p->luar_tint_upval[slot] = upval;
  p->luar_tint_acc[slot] = acc;
  p->luar_tint_res[slot] = res;
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
  p->luar_tint_suspend[slot] = 0;
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
  p->luar_tint_suspend[slot] = 0;
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
  p->luar_tint_suspend[slot] = 0;
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
  p->luar_tint_suspend[slot] = 0;
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
  p->luar_tint_suspend[slot] = 0;
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
                              int for_a, int table, int keys, int upval,
                              int key_temp) {
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
  p->luar_tint_suspend[slot] = 0;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = table;
  p->luar_tint_keys[slot] = keys;
  p->luar_tint_upval[slot] = upval;
  p->luar_tint_acc[slot] = 0;
  p->luar_tint_arg4[slot] = key_temp;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


/* LUAR: `t[i] = i + k`, `t[i] = i * k` and `t[i] = k` over a raw table
** array.  The scalar is a small integer constant held in
** luar_tint_upval; mode 1 adds it to the index, mode 2 multiplies, and
** mode 3 stores it directly.  Larger or floating scalars stay on the
** interpreter. */
#define LUAR_TINT_FILL_ADD 1
#define LUAR_TINT_FILL_MUL 2
#define LUAR_TINT_FILL_CONST 3
#define LUAR_TINT_FILL_SUB 4

static int install_affine_fill (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                                int for_a, int table, int mode,
                                int scalar) {
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
  /* Kind 29; kind 4 is the keyed sum (see install_keyed_sum).  Sharing
  ** that number made every keyed sum run this fill helper. */
  p->luar_tint[slot] = 29;
  p->luar_tint_suspend[slot] = 0;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = table;
  p->luar_tint_keys[slot] = mode;
  p->luar_tint_upval[slot] = scalar;
  p->luar_tint_acc[slot] = 0;
  p->luar_tint_res[slot] = 0;
  p->luar_tint_arg4[slot] = 0;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


/* Install a bounded cdata store helper in one of the two intrinsic slots.
** Kinds 20-23 are the store intrinsics; the existing kinds keep their
** historical encodings. */
static int install_cdata_tint (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                               int kind, int table, int keys, int upval,
                               int acc, int res, int arg4, int base) {
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
  p->luar_tint[slot] = kind;
  p->luar_tint_suspend[slot] = 0;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = table;
  p->luar_tint_keys[slot] = keys;
  p->luar_tint_upval[slot] = upval;
  p->luar_tint_acc[slot] = acc;
  p->luar_tint_res[slot] = res;
  p->luar_tint_arg4[slot] = arg4;
  p->luar_tint_base[slot] = base;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


/* LUAR: `dst[i] = src[i] +|-|*|/ k` over raw tables.  Kind 42 keeps the
** destination register, scalar encoding, value temp and operation. */
static int install_table_nil_store (Proto *p, LuaR_JIT *J,
                                     ptrdiff_t backedge, int for_a,
                                     int dst) {
  return install_cdata_tint(p, J, backedge, 51, dst, 0, 0, 0, 0, 0, for_a);
}


static int install_table_scaled_store (Proto *p, LuaR_JIT *J,
                                       ptrdiff_t backedge, int for_a,
                                       int dst, int src_enc, int scalar_enc,
                                       int scalar_imm, int elem_reg,
                                       int res_reg, int op) {
  int regs = (elem_reg & 0xff) | ((res_reg & 0xff) << 8);
  return install_cdata_tint(p, J, backedge, 42, dst, scalar_enc, regs,
                            op, scalar_imm, src_enc, for_a);
}


/* Match a cdata fill whose destination array is a closure upvalue:
**   GETUPVAL a, U; SETTABLE a, key, value
** The upvalue is re-resolved on every entry, so rebinding it is observed.
** Only the pure fill is handled here; copy/update shapes reload the array
** twice and stay on the interpreter. */
static int match_cdata_upval_fill (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                                   ptrdiff_t backedge, int for_a,
                                   TValue *b) {
  ptrdiff_t n = backedge - start + 1;
  Instruction up, st;
  int treg, arr, key, mode, arg4 = 0;
  TValue *av;
  if (n != 3)
    return 0;
  up = p->code[start];
  st = p->code[start + 1];
  if (GET_OPCODE(up) != OP_GETUPVAL ||
      GET_OPCODE(st) != OP_SETTABLE)
    return 0;
  treg = GETARG_A(up);
  arr = GETARG_A(st);
  key = GETARG_B(st);
  if (arr != treg || key != for_a + 2 ||
      is_control_reg(treg, for_a))
    return 0;
  av = pair_source_value(b, -1, GETARG_B(up));
  if (av == NULL || luar_ffi_trace_array_store_kind(av) == 0)
    return 0;
  if (GETARG_k(st)) {
    int kidx = GETARG_C(st);
    if (kidx < 0 || kidx >= p->sizek ||
        (!ttisinteger(&p->k[kidx]) && !ttisfloat(&p->k[kidx])))
      return 0;
    mode = -2;
    arg4 = kidx;
  }
  else {
    int vreg = GETARG_C(st);
    if (vreg == key)
      mode = -1;
    else {
      if (is_control_reg(vreg, for_a) ||
          (!ttisinteger(&b[vreg]) && !ttisfloat(&b[vreg])))
        return 0;
      mode = vreg;
    }
  }
  return install_cdata_tint(p, J, backedge, 25, -1 - GETARG_B(up),
                            mode, treg, 0, 0, arg4, for_a);
}


/* Match an in-place cdata update whose destination is a closure upvalue:
**   GETUPVAL d, U; GETUPVAL t, U; GETTABLE t, t, key;
**   <op> t, t, scalar; MMBIN; SETTABLE d, key, t
** The interpreter loads the upvalue twice; both reads observe the same
** binding within one iteration, so the helper re-resolves it once per
** entry.  Rebinding the upvalue to a non-array makes the helper decline
** and the interpreter reports the original error. */
static int match_cdata_upval_update (Proto *p, LuaR_JIT *J,
                                     ptrdiff_t start, ptrdiff_t backedge,
                                     int for_a, TValue *b) {
  ptrdiff_t pos = start;
  Instruction up1, up2, g, opins;
  int key = for_a + 2, arr, tmp, op = -1;
  int scalar = -1, scalar_const = -1, unary = 0;
  TValue *av;
  if (backedge - start + 1 < 6)
    return 0;
  up1 = p->code[pos++];
  up2 = p->code[pos++];
  g = p->code[pos++];
  opins = p->code[pos++];
  if (GET_OPCODE(up1) != OP_GETUPVAL ||
      GET_OPCODE(up2) != OP_GETUPVAL ||
      GETARG_B(up1) != GETARG_B(up2))
    return 0;
  arr = GETARG_A(up1);
  tmp = GETARG_A(up2);
  if (arr == tmp || arr == key || tmp == key ||
      is_control_reg(arr, for_a) || is_control_reg(tmp, for_a))
    return 0;
  if (GET_OPCODE(g) != OP_GETTABLE || GETARG_A(g) != tmp ||
      GETARG_B(g) != tmp || GETARG_C(g) != key)
    return 0;
  switch (GET_OPCODE(opins)) {
    case OP_ADD: case OP_ADDI: case OP_ADDK: op = 0; break;
    case OP_SUB: case OP_SUBK: op = 1; break;
    case OP_MUL: case OP_MULK: op = 2; break;
    case OP_DIV: case OP_DIVK: op = 3; break;
    case OP_IDIV: case OP_IDIVK: op = 4; break;
    case OP_MOD: case OP_MODK: op = 5; break;
    case OP_BAND: case OP_BANDK: op = 6; break;
    case OP_BOR: case OP_BORK: op = 7; break;
    case OP_BXOR: case OP_BXORK: op = 8; break;
    case OP_SHL: op = 9; break;
    case OP_SHR: case OP_SHRI: op = 10; break;
    case OP_BNOT: op = 11; break;
    case OP_UNM: op = 12; break;
    default: return 0;
  }
  if (GETARG_A(opins) != tmp)
    return 0;
  if (op == 11 || op == 12) {
    if (GETARG_B(opins) != tmp)
      return 0;
    unary = 1;
    scalar = -2;
    scalar_const = 0;
  }
  else if (GET_OPCODE(opins) == OP_ADD || GET_OPCODE(opins) == OP_SUB ||
           GET_OPCODE(opins) == OP_MUL || GET_OPCODE(opins) == OP_DIV ||
           GET_OPCODE(opins) == OP_IDIV || GET_OPCODE(opins) == OP_MOD ||
           GET_OPCODE(opins) == OP_BAND || GET_OPCODE(opins) == OP_BOR ||
           GET_OPCODE(opins) == OP_BXOR || GET_OPCODE(opins) == OP_SHL ||
           GET_OPCODE(opins) == OP_SHR) {
    OpCode opc = GET_OPCODE(opins);
    int bop = GETARG_B(opins), cop = GETARG_C(opins);
    int other;
    if (bop == tmp)
      other = cop;
    else if (cop == tmp && (opc == OP_ADD || opc == OP_MUL))
      other = bop;   /* compiler swapped a commutative form */
    else
      return 0;
    if (other == key || other == for_a || other == for_a + 1 ||
        other == for_a + 2 || other == arr || other == tmp ||
        (!ttisinteger(&b[other]) && !ttisfloat(&b[other])))
      return 0;
    scalar = other;
  }
  else if (GET_OPCODE(opins) == OP_ADDI ||
           GET_OPCODE(opins) == OP_SHRI) {
    if (GETARG_B(opins) != tmp)
      return 0;
    scalar = -2;
    scalar_const = (int)GETARG_sC(opins);
  }
  else {
    int idx = GETARG_C(opins);
    TValue *k;
    if (GETARG_B(opins) != tmp || idx < 0 || idx >= p->sizek)
      return 0;
    k = &p->k[idx];
    if (!ttisinteger(k) && !ttisfloat(k))
      return 0;
    scalar = -1;
    scalar_const = idx;
  }
  av = pair_source_value(b, -1, GETARG_B(up1));
  if (av == NULL || luar_ffi_trace_array_kind(av) == 0)
    return 0;
  if (op >= 6 && op <= 10 && luar_ffi_trace_array_kind(av) != 1)
    return 0;
  if (!unary) {
    OpCode mm = GET_OPCODE(p->code[pos++]);
    if (mm != OP_MMBIN && mm != OP_MMBINI && mm != OP_MMBINK)
      return 0;
  }
  if (pos >= backedge || GET_OPCODE(p->code[pos]) != OP_SETTABLE ||
      GETARG_A(p->code[pos]) != arr || GETARG_B(p->code[pos]) != key ||
      GETARG_C(p->code[pos]) != tmp || GETARG_k(p->code[pos]) ||
      pos + 1 != backedge)
    return 0;
  return install_cdata_tint(p, J, backedge, 27, -1 - GETARG_B(up1),
                            scalar, op, arr, tmp, scalar_const, for_a);
}


/* Match the three cdata store shapes:
**   a[i] = value                 (value slot, key or numeric constant)
**   a[i] = b[i]                  (element-wise copy)
**   a[i] = a[i] OP scalar        (in-place arithmetic)
** The caller has already checked the integer loop control and the body
** length.  All rejected shapes fall back to the interpreter. */
static int match_cdata_store (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                              ptrdiff_t backedge, int for_a, TValue *b) {
  ptrdiff_t n = backedge - start + 1;
  Instruction i0;
  int key = for_a + 2;
  if (n < 2)
    return 0;
  i0 = p->code[start];
  /* a[i] = value: one SETTABLE body instruction. */
  if (n == 2 && GET_OPCODE(i0) == OP_SETTABLE) {
    int arr = GETARG_A(i0), kb = GETARG_B(i0), kc = GETARG_C(i0);
    if (kb != key || arr == for_a || arr == for_a + 1 || arr == for_a + 2 ||
        luar_ffi_trace_array_store_kind(&b[arr]) == 0)
      return 0;
    if (GETARG_k(i0)) {
      TValue *k;
      if (kc < 0 || kc >= p->sizek)
        return 0;
      k = &p->k[kc];
      if (!ttisinteger(k) && !ttisfloat(k))
        return 0;
      return install_cdata_tint(p, J, backedge, 20, arr, -2, 0, 0, key,
                                kc, for_a);
    }
    if (kc == key)
      return install_cdata_tint(p, J, backedge, 20, arr, -1, 0, 0, key,
                                0, for_a);
    if (kc == arr || kc == for_a || kc == for_a + 1)
      return 0;
    if (luar_ffi_trace_i64_scalar(&b[kc]) &&
        luar_ffi_trace_array_kind64(&b[arr]) != 0)
      return install_cdata_tint(p, J, backedge, 66, arr, kc, 0, 0, 0,
                                0, for_a);
    if (!ttisinteger(&b[kc]) && !ttisfloat(&b[kc]))
      return 0;
    return install_cdata_tint(p, J, backedge, 20, arr, kc, 0, 0, key,
                              0, for_a);
  }
  /* a[i] = <cdata int64 upvalue value>. */
  if (n == 3 && GET_OPCODE(i0) == OP_GETUPVAL &&
      GET_OPCODE(p->code[start + 1]) == OP_SETTABLE) {
    Instruction up = i0, st = p->code[start + 1];
    int vreg = GETARG_A(up), dst = GETARG_A(st);
    int venc = -1 - GETARG_B(up);
    TValue *vv = pair_source_value((TValue *)b, -1, -1 - venc);
    if (GETARG_B(st) != for_a + 2 || GETARG_C(st) != vreg ||
        GETARG_k(st) || is_control_reg(vreg, for_a) ||
        is_control_reg(dst, for_a) || dst == vreg ||
        luar_ffi_trace_array_kind64(&b[dst]) == 0 ||
        vv == NULL || !luar_ffi_trace_i64_scalar(vv))
      return 0;
    return install_cdata_tint(p, J, backedge, 67, dst, venc, 0, 0, 0,
                              0, for_a);
  }
  /* a[i] = U[i] with an upvalue source. */
  if (n == 4 && GET_OPCODE(i0) == OP_GETUPVAL &&
      GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
      GET_OPCODE(p->code[start + 2]) == OP_SETTABLE) {
    Instruction up = i0, g = p->code[start + 1], s = p->code[start + 2];
    int tmp = GETARG_A(g), src = GETARG_A(up);
    int dst = GETARG_A(s), sk = GETARG_B(s), sv = GETARG_C(s);
    int src_enc = -1 - GETARG_B(up);
    if (GETARG_B(g) != src || GETARG_C(g) != key ||
        sk != key || sv != tmp || GETARG_k(s) ||
        tmp == key || tmp == for_a || tmp == for_a + 1 ||
        tmp == for_a + 2 || src == for_a || src == for_a + 1 ||
        src == for_a + 2 || dst == for_a || dst == for_a + 1 ||
        dst == for_a + 2 || dst == tmp ||
        luar_ffi_trace_array_store_kind(&b[dst]) == 0)
      return 0;
    {
      TValue *sv2 = pair_source_value((TValue *)b, -1, -1 - src_enc);
      if (sv2 != NULL && luar_ffi_trace_array_kind64(sv2) != 0)
        return install_cdata_tint(p, J, backedge, 65, dst, src_enc, tmp,
                                  0, 0, 0, for_a);
      if (sv2 == NULL || luar_ffi_trace_array_kind(sv2) == 0)
        return 0;
    }
    return install_cdata_tint(p, J, backedge, 21, dst, src_enc, 0, tmp,
                              key, 0, for_a);
  }
  /* a[i] = b[i] for 8-byte integer arrays: raw 8-byte copy with a cdata
  ** element temporary. */
  if (n == 3 && GET_OPCODE(i0) == OP_GETTABLE &&
      GET_OPCODE(p->code[start + 1]) == OP_SETTABLE) {
    Instruction g = i0, s = p->code[start + 1];
    int tmp = GETARG_A(g), src = GETARG_B(g);
    int dst = GETARG_A(s), sk = GETARG_B(s), sv = GETARG_C(s);
    if (GETARG_C(g) != key || sk != key || sv != tmp ||
        GETARG_k(s) || tmp == key || tmp == for_a || tmp == for_a + 1 ||
        tmp == for_a + 2 || src == for_a || src == for_a + 1 ||
        src == for_a + 2 || dst == for_a || dst == for_a + 1 ||
        dst == for_a + 2 || dst == tmp || src == tmp ||
        luar_ffi_trace_array_store_kind(&b[dst]) == 0)
      return 0;
    if (luar_ffi_trace_array_kind64(&b[src]) != 0)
      return install_cdata_tint(p, J, backedge, 64, dst, src, tmp,
                                0, 0, 0, for_a);
  }
  /* a[i] = b[i]: GETTABLE tmp, src, key; SETTABLE dst, key, tmp. */
  if (n == 3 && GET_OPCODE(i0) == OP_GETTABLE &&
      GET_OPCODE(p->code[start + 1]) == OP_SETTABLE) {
    Instruction g = i0, s = p->code[start + 1];
    int tmp = GETARG_A(g), src = GETARG_B(g);
    int dst = GETARG_A(s), sk = GETARG_B(s), sv = GETARG_C(s);
    if (GETARG_C(g) != key || sk != key || sv != tmp ||
        GETARG_k(s) || tmp == key || tmp == for_a || tmp == for_a + 1 ||
        tmp == for_a + 2 || src == for_a || src == for_a + 1 ||
        src == for_a + 2 || dst == for_a || dst == for_a + 1 ||
        dst == for_a + 2 || dst == tmp || src == tmp ||
        luar_ffi_trace_array_store_kind(&b[dst]) == 0 ||
        luar_ffi_trace_array_kind(&b[src]) == 0)
      return 0;
    return install_cdata_tint(p, J, backedge, 21, dst, src, 0, tmp, key,
                              0, for_a);
  }
  /* a[i] = a[i] OP cdata-scalar for 8-byte integer arrays. */
  if (n == 5 && GET_OPCODE(i0) == OP_GETTABLE &&
      GET_OPCODE(p->code[start + 2]) == OP_MMBIN &&
      GET_OPCODE(p->code[start + 3]) == OP_SETTABLE &&
      backedge == start + 4) {
    Instruction g = i0, op = p->code[start + 1], s = p->code[start + 3];
    int tmp = GETARG_A(g), arr = GETARG_B(g);
    int scalar = GETARG_C(op), dst = GETARG_A(s), opc = GET_OPCODE(op);
    int code;
    if (GETARG_C(g) != key || GETARG_A(op) != tmp ||
        GETARG_B(op) != tmp || dst != arr || GETARG_B(s) != key ||
        GETARG_C(s) != tmp || GETARG_k(s) ||
        tmp == key || arr == for_a || arr == for_a + 1 ||
        arr == for_a + 2 || tmp == for_a || tmp == for_a + 1 ||
        tmp == for_a + 2 || scalar == key || scalar == arr ||
        scalar == tmp || is_control_reg(scalar, for_a) ||
        luar_ffi_trace_array_kind64(&b[arr]) == 0 ||
        !luar_ffi_trace_i64_scalar(&b[scalar]))
      return 0;
    if (opc == OP_ADD) code = 0;
    else if (opc == OP_SUB) code = 1;
    else if (opc == OP_MUL) code = 2;
    else if (opc == OP_BAND) code = 6;
    else if (opc == OP_BOR) code = 7;
    else if (opc == OP_BXOR) code = 8;
    else if (opc == OP_SHL) code = 9;
    else if (opc == OP_SHR) code = 10;
    else if (opc == OP_IDIV) code = 3;
    else if (opc == OP_MOD) code = 4;
    else return 0;
    return install_cdata_tint(p, J, backedge, 71, arr, scalar, tmp,
                              code, 0, 0, for_a);
  }
  /* a[i] = a[i] OP upvalue cdata scalar for 8-byte integer arrays. */
  if (n == 6 && GET_OPCODE(i0) == OP_GETTABLE &&
      GET_OPCODE(p->code[start + 1]) == OP_GETUPVAL &&
      (GET_OPCODE(p->code[start + 2]) == OP_ADD ||
       GET_OPCODE(p->code[start + 2]) == OP_SUB ||
       GET_OPCODE(p->code[start + 2]) == OP_MUL ||
       GET_OPCODE(p->code[start + 2]) == OP_BAND ||
       GET_OPCODE(p->code[start + 2]) == OP_BOR ||
       GET_OPCODE(p->code[start + 2]) == OP_BXOR ||
       GET_OPCODE(p->code[start + 2]) == OP_SHL ||
       GET_OPCODE(p->code[start + 2]) == OP_SHR ||
       GET_OPCODE(p->code[start + 2]) == OP_IDIV ||
       GET_OPCODE(p->code[start + 2]) == OP_MOD) &&
      GET_OPCODE(p->code[start + 3]) == OP_MMBIN &&
      GET_OPCODE(p->code[start + 4]) == OP_SETTABLE &&
      backedge == start + 5) {
    Instruction g = i0, up = p->code[start + 1];
    Instruction op = p->code[start + 2], s = p->code[start + 4];
    int tmp = GETARG_A(g), arr = GETARG_B(g), sreg = GETARG_A(up);
    int dst = GETARG_A(s), opc = GET_OPCODE(op), code;
    TValue *sv = pair_source_value((TValue *)b, -1, GETARG_B(up));
    if (GETARG_C(g) != key || GETARG_A(op) != tmp ||
        GETARG_B(op) != tmp || GETARG_C(op) != sreg ||
        GETARG_B(s) != key || GETARG_C(s) != tmp || GETARG_k(s) ||
        dst != arr || tmp == key || arr == for_a || arr == for_a + 1 ||
        arr == for_a + 2 || tmp == for_a || tmp == for_a + 1 ||
        tmp == for_a + 2 || is_control_reg(sreg, for_a) ||
        luar_ffi_trace_array_kind64(&b[arr]) == 0 ||
        sv == NULL || !luar_ffi_trace_i64_scalar(sv))
      return 0;
    if (opc == OP_ADD) code = 0;
    else if (opc == OP_SUB) code = 1;
    else if (opc == OP_MUL) code = 2;
    else if (opc == OP_BAND) code = 6;
    else if (opc == OP_BOR) code = 7;
    else if (opc == OP_BXOR) code = 8;
    else if (opc == OP_SHL) code = 9;
    else if (opc == OP_SHR) code = 10;
    else if (opc == OP_IDIV) code = 3;
    else code = 4;
    return install_cdata_tint(p, J, backedge, 74, arr, -1 - GETARG_B(up),
                              tmp, code, 0, 0, for_a);
  }
  /* a[i] = a[i] OP small integer for 8-byte integer arrays. */
  if (n == 5 && GET_OPCODE(i0) == OP_GETTABLE &&
      (GET_OPCODE(p->code[start + 2]) == OP_MMBIN ||
       GET_OPCODE(p->code[start + 2]) == OP_MMBINI ||
       GET_OPCODE(p->code[start + 2]) == OP_MMBINK) &&
      GET_OPCODE(p->code[start + 3]) == OP_SETTABLE &&
      backedge == start + 4) {
    Instruction g = i0, op = p->code[start + 1], s = p->code[start + 3];
    int tmp = GETARG_A(g), arr = GETARG_B(g);
    int dst = GETARG_A(s), opc = GET_OPCODE(op), code = -1;
    int64_t scalar = 0;
    if (GETARG_C(g) != key || GETARG_A(op) != tmp ||
        GETARG_B(op) != tmp || dst != arr || GETARG_B(s) != key ||
        GETARG_C(s) != tmp || GETARG_k(s) ||
        tmp == key || arr == for_a || arr == for_a + 1 ||
        arr == for_a + 2 || tmp == for_a || tmp == for_a + 1 ||
        tmp == for_a + 2 || luar_ffi_trace_array_kind64(&b[arr]) == 0)
      return 0;
    if (opc == OP_ADDI) {
      code = 0; scalar = GETARG_sC(op);
    }
    else if (opc == OP_SHLI) {
      code = 9; scalar = GETARG_sC(op);
    }
    else if (opc == OP_SHRI) {
      int64_t n = GETARG_sC(op);
      code = (n < 0) ? 9 : 10;
      scalar = (n < 0) ? -n : n;
    }
    else if (opc == OP_ADDK || opc == OP_SUBK || opc == OP_MULK ||
             opc == OP_BANDK || opc == OP_BORK || opc == OP_BXORK ||
             opc == OP_IDIVK || opc == OP_MODK) {
      int idx = GETARG_C(op);
      if (idx < 0 || idx >= p->sizek || !ttisinteger(&p->k[idx]))
        return 0;
      scalar = ivalue(&p->k[idx]);
      code = (opc == OP_ADDK) ? 0 : (opc == OP_SUBK) ? 1 :
             (opc == OP_MULK) ? 2 : (opc == OP_IDIVK) ? 3 :
             (opc == OP_MODK) ? 4 : (opc == OP_BANDK) ? 6 :
             (opc == OP_BORK) ? 7 : 8;
    }
    else
      return 0;
    if (scalar < INT32_MIN || scalar > INT32_MAX)
      return 0;
    return install_cdata_tint(p, J, backedge, 73, arr, 0, tmp,
                              code, 0, (int)scalar, for_a);
  }
  /* a[i] = a[i] OP scalar: GETTABLE tmp, arr, key; <op>; SETTABLE. */
  if (n >= 4 && GET_OPCODE(i0) == OP_GETTABLE) {
    Instruction g = i0;
    int tmp = GETARG_A(g), arr = GETARG_B(g);
    Instruction opins = p->code[start + 1];
    OpCode opc = GET_OPCODE(opins);
    int op;
    int scalar = -1, scalar_const = -1;
    ptrdiff_t spi;
    if (GETARG_C(g) != key || arr == for_a || arr == for_a + 1 ||
        arr == for_a + 2 || tmp == key || tmp == arr || tmp == for_a ||
        tmp == for_a + 1 || tmp == for_a + 2 ||
        luar_ffi_trace_array_kind(&b[arr]) == 0)
      return 0;
    switch (opc) {
      case OP_ADD: case OP_ADDI: case OP_ADDK: op = 0; break;
      case OP_SUB: case OP_SUBK: op = 1; break;
      case OP_MUL: case OP_MULK: op = 2; break;
      case OP_DIV: case OP_DIVK: op = 3; break;
      case OP_IDIV: case OP_IDIVK: op = 4; break;
      case OP_MOD: case OP_MODK: op = 5; break;
      case OP_BAND: case OP_BANDK: op = 6; break;
      case OP_BOR: case OP_BORK: op = 7; break;
      case OP_BXOR: case OP_BXORK: op = 8; break;
      case OP_SHL: op = 9; break;
      case OP_SHR: case OP_SHRI: op = 10; break;
      case OP_BNOT: op = 11; break;
      case OP_UNM: op = 12; break;
      default: return 0;
    }
    /* The arithmetic destination must be the GETTABLE temporary, so the
    ** final SETTABLE stores the computed value. */
    if (GETARG_A(opins) != tmp)
      return 0;
    /* Integer bitwise operations are only defined on an integer view.  A
    ** float array element would need a per-element integer-conversion check
    ** before any store, so leave those loops to the interpreter. */
    if (op >= 6 && luar_ffi_trace_array_kind(&b[arr]) != 1)
      return 0;
    if (opc == OP_ADD || opc == OP_SUB || opc == OP_MUL || opc == OP_DIV ||
        opc == OP_IDIV || opc == OP_MOD || opc == OP_BAND || opc == OP_BOR ||
        opc == OP_BXOR || opc == OP_SHL || opc == OP_SHR) {
      int bop = GETARG_B(opins), cop = GETARG_C(opins);
      if (bop != tmp)
        return 0;
      if (cop == key || cop == for_a || cop == for_a + 1 ||
          cop == for_a + 2 || cop == arr || cop == tmp ||
          (!ttisinteger(&b[cop]) && !ttisfloat(&b[cop])))
        return 0;
      scalar = cop;
      spi = start + 2;
    }
    else if (opc == OP_ADDI || opc == OP_SHRI) {
      if (GETARG_B(opins) != tmp)
        return 0;
      scalar = -2;
      scalar_const = (int)GETARG_sC(opins);
      spi = start + 2;
    }
    else if (opc == OP_BNOT || opc == OP_UNM) {
      if (GETARG_B(opins) != tmp)
        return 0;
      /* Unary operators have no following MMBIN: the interpreter handles
      ** their metamethod directly in the opcode.  Keep a dummy scalar so
      ** the dispatch never indexes a constant for these ops. */
      scalar = -2;
      scalar_const = 0;
      spi = start + 2;
      if (spi >= backedge || GET_OPCODE(p->code[spi]) != OP_SETTABLE ||
          spi + 1 != backedge)
        return 0;
      goto cdata_store_check;
    }
    else {   /* OP_ADDK/SUBK/MULK/DIVK/BANDK/BORK/BXORK */
      int idx = GETARG_C(opins);
      TValue *k;
      if (GETARG_B(opins) != tmp || idx < 0 || idx >= p->sizek)
        return 0;
      k = &p->k[idx];
      if (!ttisinteger(k) && !ttisfloat(k))
        return 0;
      scalar = -1;
      scalar_const = idx;
      spi = start + 2;
    }
    /* MM(BIN/I/K) follows the arithmetic on the interpreter's error path;
    ** require it and then the final SETTABLE. */
    if (spi >= backedge ||
        (GET_OPCODE(p->code[spi]) != OP_MMBIN &&
         GET_OPCODE(p->code[spi]) != OP_MMBINI &&
         GET_OPCODE(p->code[spi]) != OP_MMBINK))
      return 0;
    spi++;
    if (spi >= backedge || GET_OPCODE(p->code[spi]) != OP_SETTABLE ||
        spi + 1 != backedge)
      return 0;
  cdata_store_check:
    {
      Instruction s = p->code[spi];
      if (GETARG_A(s) != arr || GETARG_B(s) != key ||
          GETARG_C(s) != tmp || GETARG_k(s))
        return 0;
    }
    return install_cdata_tint(p, J, backedge, 22, arr, scalar, op, tmp, key,
                              scalar_const, for_a);
  }
  return 0;
}


/* Match `a[i] = b[i] OP v` and its immediate/constant forms: GETTABLE
** tmp, src, key; arithmetic tmp = tmp OP scalar; MMBIN; SETTABLE.  Only
** source-OP-scalar order is accepted; the compiler already swaps the
** commutative add/multiply forms. */
static int match_cdata_scalar_store (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                                     ptrdiff_t backedge, int for_a,
                                     TValue *b) {
  ptrdiff_t pos = start;
  Instruction g, opins, mm, s;
  int key = for_a + 2;
  int tmp, src, dst, op, scalar = -1, scalar_const = -1;
  int up_enc = 0, up_reg = -1, src_up_enc = 0, src_up_reg = -1, src_enc = -1;
  OpCode opc;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    src_up_reg = GETARG_A(up);
    if (is_control_reg(src_up_reg, for_a))
      return 0;
    src_up_enc = -1 - GETARG_B(up);
  }
  if (pos >= backedge || GET_OPCODE(p->code[pos]) != OP_GETTABLE)
    return 0;
  g = p->code[pos++];
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  if (src_up_enc != 0) {
    if (src != src_up_reg)
      return 0;
    src_enc = src_up_enc;
  }
  else
    src_enc = src;
  if (GETARG_C(g) != key || tmp == key || src == key ||
      (src_up_enc == 0 && tmp == src) ||
      is_control_reg(tmp, for_a) || is_control_reg(src, for_a))
    return 0;
  {
    TValue *sv = pair_source_value(b, (src_enc >= 0) ? src_enc : -1,
                                   (src_enc >= 0) ? -1 : (-1 - src_enc));
    if (sv == NULL || luar_ffi_trace_array_kind(sv) == 0)
      return 0;
  }
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    up_reg = GETARG_A(up);
    if (is_control_reg(up_reg, for_a) || up_reg == tmp ||
        up_reg == src || up_reg == key)
      return 0;
    up_enc = -3 - GETARG_B(up);
  }
  if (pos >= backedge)
    return 0;
  opins = p->code[pos++];
  opc = GET_OPCODE(opins);
  switch (opc) {
    case OP_ADD: case OP_ADDI: case OP_ADDK: op = 0; break;
    case OP_SUB: case OP_SUBK: op = 1; break;
    case OP_MUL: case OP_MULK: op = 2; break;
    case OP_DIV: case OP_DIVK: op = 3; break;
    case OP_IDIV: case OP_IDIVK: op = 4; break;
    case OP_MOD: case OP_MODK: op = 5; break;
    case OP_BAND: case OP_BANDK: op = 6; break;
    case OP_BOR: case OP_BORK: op = 7; break;
    case OP_BXOR: case OP_BXORK: op = 8; break;
    case OP_SHL: case OP_SHLI: op = 9; break;
    case OP_SHR: case OP_SHRI: op = 10; break;
    default: return 0;
  }
  if (GETARG_A(opins) != tmp)
    return 0;
  if (opc == OP_ADD || opc == OP_SUB || opc == OP_MUL || opc == OP_DIV ||
      opc == OP_IDIV || opc == OP_MOD ||
      opc == OP_BAND || opc == OP_BOR || opc == OP_BXOR ||
      opc == OP_SHL || opc == OP_SHR) {
    int bop = GETARG_B(opins), cop = GETARG_C(opins);
    int other;
    if (bop == tmp)
      other = cop;
    else if (cop == tmp && (opc == OP_ADD || opc == OP_MUL ||
                            opc == OP_BAND || opc == OP_BOR ||
                            opc == OP_BXOR))
      other = bop;
    else
      return 0;
    if (other == key || other == for_a || other == for_a + 1 ||
        other == for_a + 2 || other == src || other == tmp)
      return 0;
    if (up_enc != 0) {
      if (other != up_reg)
        return 0;
      scalar = up_enc;
    }
    else {
      if (!ttisinteger(&b[other]) && !ttisfloat(&b[other]))
        return 0;
      scalar = other;
    }
  }
  else {
    if (up_enc != 0)
      return 0;
    if (opc == OP_ADDI || opc == OP_SHLI || opc == OP_SHRI) {
      if (GETARG_B(opins) != tmp)
        return 0;
      scalar = -2;
      scalar_const = (int)GETARG_sC(opins);
    }
    else {
      int idx = GETARG_C(opins);
      TValue *k;
      if (GETARG_B(opins) != tmp || idx < 0 || idx >= p->sizek)
        return 0;
      k = &p->k[idx];
      if (!ttisinteger(k) && !ttisfloat(k))
        return 0;
      scalar = -1;
      scalar_const = idx;
    }
  }
  if (pos >= backedge)
    return 0;
  mm = p->code[pos++];
  if (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
      GET_OPCODE(mm) != OP_MMBINK)
    return 0;
  if (pos >= backedge)
    return 0;
  s = p->code[pos++];
  if (GET_OPCODE(s) != OP_SETTABLE || GETARG_k(s) || GETARG_B(s) != key ||
      GETARG_C(s) != tmp)
    return 0;
  dst = GETARG_A(s);
  if (dst == tmp || dst == for_a || dst == for_a + 1 ||
      dst == for_a + 2 || (up_enc != 0 && dst == up_reg) ||
      luar_ffi_trace_array_store_kind(&b[dst]) == 0)
    return 0;
  if (pos != backedge)
    return 0;
  return install_cdata_tint(p, J, backedge, 24, dst, src_enc, scalar, op,
                            tmp, scalar_const, for_a);
}


/* Match `a[i] = -b[i]` / `a[i] = ~b[i]` over bounded cdata arrays. */
static int match_cdata_unary_store (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                                    ptrdiff_t backedge, int for_a,
                                    TValue *b) {
  ptrdiff_t pos = start;
  Instruction g, opins, s;
  int key = for_a + 2, tmp, src, dst, op, res, src_up_enc = 0, src_up_reg = -1;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    src_up_reg = GETARG_A(up);
    if (is_control_reg(src_up_reg, for_a))
      return 0;
    src_up_enc = -1 - GETARG_B(up);
  }
  if (pos >= backedge || GET_OPCODE(p->code[pos]) != OP_GETTABLE)
    return 0;
  g = p->code[pos++];
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  if (src_up_enc != 0 && src != src_up_reg)
    return 0;
  if (GETARG_C(g) != key || tmp == key ||
      (src_up_enc == 0 && tmp == src) ||
      is_control_reg(tmp, for_a) || is_control_reg(src, for_a))
    return 0;
  {
    int src_enc = (src_up_enc != 0) ? src_up_enc : src;
    TValue *sv = pair_source_value(b, (src_enc >= 0) ? src_enc : -1,
                                   (src_enc >= 0) ? -1 : (-1 - src_enc));
    if (sv == NULL || luar_ffi_trace_array_kind(sv) == 0)
      return 0;
    if (pos >= backedge)
      return 0;
    opins = p->code[pos++];
    if (GET_OPCODE(opins) == OP_UNM)
      op = 0;
    else if (GET_OPCODE(opins) == OP_BNOT)
      op = 1;
    else
      return 0;
    res = GETARG_A(opins);
    if (GETARG_B(opins) != tmp || is_control_reg(res, for_a))
      return 0;
    if (pos >= backedge)
      return 0;
    s = p->code[pos++];
    if (GET_OPCODE(s) != OP_SETTABLE || GETARG_k(s) ||
        GETARG_B(s) != key || GETARG_C(s) != res)
      return 0;
    dst = GETARG_A(s);
    if (dst == tmp || dst == res || dst == for_a || dst == for_a + 1 ||
        dst == for_a + 2 ||
        luar_ffi_trace_array_store_kind(&b[dst]) == 0)
      return 0;
    if (pos != backedge)
      return 0;
    return install_cdata_tint(p, J, backedge, 56, dst, src_enc, tmp, op,
                              res, 0, for_a);
  }
}


/* Match `a[i] = b[i] OP c[i]`: GETTABLE ta, b, key; GETTABLE tb, c, key;
** arithmetic; SETTABLE.  The arithmetic destination must be ta, which is
** the common compiler shape. */
static int match_cdata_pair_store (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                                   ptrdiff_t backedge, int for_a, TValue *b) {
  ptrdiff_t pos = start;
  Instruction g1, g2, opins, mm, s;
  int key = for_a + 2, op;
  int ta, tb, av, bv, aenc, benc, aup = 0, bup = 0;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    aup = -1 - GETARG_B(up);
    av = GETARG_A(up);
  }
  if (pos >= backedge || GET_OPCODE(p->code[pos]) != OP_GETTABLE)
    return 0;
  g1 = p->code[pos++];
  ta = GETARG_A(g1);
  if (aup != 0) {
    if (GETARG_B(g1) != av)
      return 0;
    aenc = aup;
  }
  else {
    av = GETARG_B(g1);
    aenc = av;
  }
  if (GETARG_C(g1) != key)
    return 0;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    bup = -1 - GETARG_B(up);
    bv = GETARG_A(up);
  }
  if (pos >= backedge || GET_OPCODE(p->code[pos]) != OP_GETTABLE)
    return 0;
  g2 = p->code[pos++];
  tb = GETARG_A(g2);
  if (bup != 0) {
    if (GETARG_B(g2) != bv)
      return 0;
    benc = bup;
  }
  else {
    bv = GETARG_B(g2);
    benc = bv;
  }
  if (GETARG_C(g2) != key)
    return 0;
  if (pos >= backedge)
    return 0;
  opins = p->code[pos++];
  if (GETARG_A(opins) != ta || GETARG_B(opins) != ta ||
      GETARG_C(opins) != tb)
    return 0;
  switch (GET_OPCODE(opins)) {
    case OP_ADD: op = 0; break;
    case OP_SUB: op = 1; break;
    case OP_MUL: op = 2; break;
    case OP_DIV: op = 3; break;
    case OP_IDIV: op = 4; break;
    case OP_MOD: op = 5; break;
    case OP_BAND: op = 6; break;
    case OP_BOR: op = 7; break;
    case OP_BXOR: op = 8; break;
    case OP_SHL: op = 9; break;
    case OP_SHR: op = 10; break;
    default: return 0;
  }
  if (pos >= backedge)
    return 0;
  mm = p->code[pos++];
  if (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
      GET_OPCODE(mm) != OP_MMBINK)
    return 0;
  if (pos >= backedge)
    return 0;
  s = p->code[pos++];
  if (GET_OPCODE(s) != OP_SETTABLE || GETARG_k(s) ||
      GETARG_B(s) != key || GETARG_C(s) != ta)
    return 0;
  {
    int dst = GETARG_A(s);
    if (dst == for_a || dst == for_a + 1 || dst == for_a + 2 ||
        dst == ta || ta == key || tb == key || ta == tb ||
        (!aup && (ta == av)) || (!bup && (tb == bv)) ||
        (!aup && (ta == bv)) || (!bup && (tb == av)) ||
        aenc == (int)key || benc == (int)key ||
        luar_ffi_trace_array_store_kind(&b[dst]) == 0)
      return 0;
    if (pos != backedge)
      return 0;
    return install_cdata_tint(p, J, backedge, 23, dst, aenc, benc, ta, tb,
                              op, for_a);
  }
}


/* LUAR: `a[i] = a[i] OP v` for float/double arrays with Lua numbers. */
static int match_cdata_array_update_f64_number (Proto *p, LuaR_JIT *J,
                                                ptrdiff_t start,
                                                ptrdiff_t backedge,
                                                int for_a, TValue *b) {
  ptrdiff_t pos = start + 1;
  Instruction i0, op, mm, s;
  int tmp, arr, key, scalar_enc = 0, scalar_reg = -1, opc, code;
  (void)b;
  if (backedge - start + 1 != 5 && backedge - start + 1 != 6)
    return 0;
  i0 = p->code[start];
  if (GET_OPCODE(i0) != OP_GETTABLE)
    return 0;
  tmp = GETARG_A(i0);
  arr = GETARG_B(i0);
  key = for_a + 2;
  if (GETARG_C(i0) != key || luar_ffi_trace_array_kind(&b[arr]) != 2 ||
      is_control_reg(tmp, for_a) || is_control_reg(arr, for_a))
    return 0;
  if (backedge - start + 1 == 6) {
    Instruction up = p->code[start + 1];
    if (GET_OPCODE(up) != OP_GETUPVAL)
      return 0;
    scalar_reg = GETARG_A(up);
    scalar_enc = -1 - GETARG_B(up);
    pos = start + 2;
  }
  if (pos + 3 != backedge)
    return 0;
  op = p->code[pos++];
  mm = p->code[pos++];
  s = p->code[pos++];
  if ((GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
       GET_OPCODE(mm) != OP_MMBINK) ||
      GET_OPCODE(s) != OP_SETTABLE || GETARG_A(s) != arr ||
      GETARG_B(s) != key || GETARG_C(s) != tmp || GETARG_k(s) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp)
    return 0;
  if (scalar_enc == 0) {
    scalar_reg = GETARG_C(op);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == arr ||
        scalar_reg == tmp)
      return 0;
    scalar_enc = scalar_reg;
  }
  else if (GETARG_C(op) != scalar_reg)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_DIV) code = 3;
  else if (opc == OP_MOD) code = 4;
  else return 0;
  return install_cdata_tint(p, J, backedge, 95, arr, code, tmp,
                            0, scalar_enc, 0, for_a);
}


/* LUAR: `a[i] = k` for 8-byte float arrays with a numeric constant. */
static int match_cdata_array_fill_f64_const (Proto *p, LuaR_JIT *J,
                                             ptrdiff_t start,
                                             ptrdiff_t backedge,
                                             int for_a, TValue *b) {
  Instruction s;
  int arr, key, kidx;
  if (start + 1 != backedge)
    return 0;
  s = p->code[start];
  if (GET_OPCODE(s) != OP_SETTABLE || !GETARG_k(s))
    return 0;
  arr = GETARG_A(s);
  key = for_a + 2;
  kidx = GETARG_C(s);
  if (GETARG_B(s) != key || kidx < 0 || kidx >= p->sizek ||
      (!ttisfloat(&p->k[kidx]) && !ttisinteger(&p->k[kidx])) ||
      luar_ffi_trace_array_kind(&b[arr]) != 2 ||
      is_control_reg(arr, for_a))
    return 0;
  return install_cdata_tint(p, J, backedge, 89, arr, 0, 0,
                            0, 0, kidx, for_a);
}


/* LUAR: `dst[i] = U[i]` for float/double arrays in an upvalue. */
static int match_cdata_array_copy_from_upval_f64 (Proto *p, LuaR_JIT *J,
                                                  ptrdiff_t start,
                                                  ptrdiff_t backedge,
                                                  int for_a, TValue *b) {
  ptrdiff_t pos = start;
  Instruction u, g, s;
  int dst, src, tmp, key;
  if (backedge - start + 1 != 4 && backedge - start + 1 != 5)
    return 0;
  u = p->code[pos];
  if (GET_OPCODE(u) != OP_GETUPVAL)
    return 0;
  if (backedge - start + 1 == 5) {
    dst = GETARG_A(u);
    pos++;
    u = p->code[pos];
    if (GET_OPCODE(u) != OP_GETUPVAL || is_control_reg(dst, for_a))
      return 0;
  }
  else
    dst = -1;
  src = GETARG_A(u);
  pos++;
  g = p->code[pos++];
  s = p->code[pos++];
  key = for_a + 2;
  if (GET_OPCODE(g) != OP_GETTABLE || GET_OPCODE(s) != OP_SETTABLE)
    return 0;
  tmp = GETARG_A(g);
  if (GETARG_B(g) != src || GETARG_C(g) != key ||
      GETARG_B(s) != key || GETARG_C(s) != tmp || GETARG_k(s))
    return 0;
  if (dst < 0)
    dst = GETARG_A(s);
  else if (GETARG_A(s) != dst)
    return 0;
  if (luar_ffi_trace_array_kind(&b[src]) != 2 ||
      luar_ffi_trace_array_kind(&b[dst]) != 2 ||
      is_control_reg(src, for_a) || is_control_reg(dst, for_a) ||
      is_control_reg(tmp, for_a))
    return 0;
  return install_cdata_tint(p, J, backedge, 94, dst, src, tmp,
                            0, 0, 0, for_a);
}


/* LUAR: `U[i] = src[i]` for float/double arrays in an upvalue. */
static int match_cdata_upval_array_copy_f64 (Proto *p, LuaR_JIT *J,
                                             ptrdiff_t start,
                                             ptrdiff_t backedge,
                                             int for_a, TValue *b) {
  Instruction u, g, s;
  int dst, src, tmp, key;
  if (start + 3 != backedge)
    return 0;
  u = p->code[start];
  g = p->code[start + 1];
  s = p->code[start + 2];
  key = for_a + 2;
  if (GET_OPCODE(u) != OP_GETUPVAL || GET_OPCODE(g) != OP_GETTABLE ||
      GET_OPCODE(s) != OP_SETTABLE)
    return 0;
  dst = GETARG_A(u);
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  if (GETARG_A(s) != dst || GETARG_B(s) != key ||
      GETARG_C(s) != tmp || GETARG_k(s) ||
      GETARG_C(g) != key ||
      luar_ffi_trace_array_kind(&b[dst]) != 2 ||
      luar_ffi_trace_array_kind(&b[src]) != 2 ||
      is_control_reg(dst, for_a) || is_control_reg(src, for_a) ||
      is_control_reg(tmp, for_a))
    return 0;
  return install_cdata_tint(p, J, backedge, 93, dst, src, tmp,
                            0, 0, 0, for_a);
}


/* LUAR: `U[i] = k` for float/double arrays held in a closure upvalue. */
static int match_cdata_upval_array_fill_f64_const (Proto *p, LuaR_JIT *J,
                                                   ptrdiff_t start,
                                                   ptrdiff_t backedge,
                                                   int for_a, TValue *b) {
  Instruction g, s;
  int arr, key, kidx;
  size_t upidx;
  TValue *fv, *cv;
  LClosure *cl;
  if (start + 2 != backedge)
    return 0;
  g = p->code[start];
  s = p->code[start + 1];
  if (GET_OPCODE(g) != OP_GETUPVAL || GET_OPCODE(s) != OP_SETTABLE ||
      !GETARG_k(s))
    return 0;
  arr = GETARG_A(g);
  upidx = (size_t)GETARG_B(g);
  key = for_a + 2;
  kidx = GETARG_C(s);
  if (GETARG_A(s) != arr || GETARG_B(s) != key || kidx < 0 ||
      kidx >= p->sizek ||
      (!ttisfloat(&p->k[kidx]) && !ttisinteger(&p->k[kidx])))
    return 0;
  fv = &b[-1];
  if (!ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  if (upidx >= (size_t)cl->p->sizeupvalues || cl->upvals[upidx] == NULL ||
      cl->upvals[upidx]->v.p == NULL)
    return 0;
  cv = cl->upvals[upidx]->v.p;
  if (luar_ffi_trace_array_kind(cv) != 2 || is_control_reg(arr, for_a))
    return 0;
  return install_cdata_tint(p, J, backedge, 92, arr, 0, 0,
                            0, 0, kidx, for_a);
}


/* LUAR: `U[i] = V` for float/double arrays held in a closure upvalue. */
static int match_cdata_upval_array_fill_f64 (Proto *p, LuaR_JIT *J,
                                             ptrdiff_t start,
                                             ptrdiff_t backedge,
                                             int for_a, TValue *b) {
  ptrdiff_t pos = start + 1;
  Instruction g, s, up;
  int arr, key, val_enc = 0;
  size_t upidx;
  TValue *fv, *cv;
  LClosure *cl;
  if (backedge - start + 1 != 3 && backedge - start + 1 != 4)
    return 0;
  g = p->code[start];
  if (GET_OPCODE(g) != OP_GETUPVAL)
    return 0;
  arr = GETARG_A(g);
  upidx = (size_t)GETARG_B(g);
  if (backedge - start + 1 == 4) {
    up = p->code[pos];
    if (GET_OPCODE(up) != OP_GETUPVAL || is_control_reg(GETARG_A(up), for_a))
      return 0;
    val_enc = -1 - GETARG_B(up);
    pos++;
  }
  s = p->code[pos];
  key = for_a + 2;
  if (GET_OPCODE(s) != OP_SETTABLE || GETARG_A(s) != arr ||
      GETARG_B(s) != key || GETARG_k(s))
    return 0;
  fv = &b[-1];
  if (!ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  if (upidx >= (size_t)cl->p->sizeupvalues || cl->upvals[upidx] == NULL ||
      cl->upvals[upidx]->v.p == NULL)
    return 0;
  cv = cl->upvals[upidx]->v.p;
  if (luar_ffi_trace_array_kind(cv) != 2)
    return 0;
  if (val_enc == 0)
    val_enc = GETARG_C(s);
  else if (GETARG_C(s) < 0)
    return 0;
  if (is_control_reg(arr, for_a) || is_control_reg(GETARG_C(s), for_a))
    return 0;
  return install_cdata_tint(p, J, backedge, 91, arr, val_enc, 0,
                            0, 0, 0, for_a);
}



/* LUAR: `a[i] = V` for 8-byte float arrays with a cdata double V. */
static int match_cdata_array_fill_f64 (Proto *p, LuaR_JIT *J,
                                       ptrdiff_t start, ptrdiff_t backedge,
                                       int for_a, TValue *b) {
  ptrdiff_t pos = start;
  Instruction s, up;
  int arr, key, dst, val_enc = 0;
  if (backedge - start + 1 != 2 && backedge - start + 1 != 3)
    return 0;
  if (backedge - start + 1 == 3) {
    up = p->code[pos];
    if (GET_OPCODE(up) != OP_GETUPVAL || is_control_reg(GETARG_A(up), for_a))
      return 0;
    val_enc = -1 - GETARG_B(up);
    pos++;
  }
  s = p->code[pos];
  if (GET_OPCODE(s) != OP_SETTABLE)
    return 0;
  arr = GETARG_A(s);
  key = for_a + 2;
  dst = arr;
  if (GETARG_B(s) != key || GETARG_k(s) ||
      luar_ffi_trace_array_kind(&b[arr]) != 2 ||
      is_control_reg(arr, for_a) || is_control_reg(GETARG_C(s), for_a))
    return 0;
  if (val_enc == 0)
    val_enc = GETARG_C(s);
  else if (GETARG_C(s) < 0)
    return 0;
  (void)dst;
  return install_cdata_tint(p, J, backedge, 88, arr, val_enc, 0,
                            0, 0, 0, for_a);
}


/* LUAR: `a[i] = a[i] OP k` for 8-byte float arrays. */
static int match_cdata_array_update_f64_const (Proto *p, LuaR_JIT *J,
                                               ptrdiff_t start,
                                               ptrdiff_t backedge,
                                               int for_a, TValue *b) {
  ptrdiff_t n = backedge - start + 1;
  Instruction i0, op, mm, s;
  int tmp, arr, dst, key, kidx, opc, code;
  if (n != 5)
    return 0;
  i0 = p->code[start];
  op = p->code[start + 1];
  mm = p->code[start + 2];
  s = p->code[start + 3];
  if (GET_OPCODE(i0) != OP_GETTABLE || GET_OPCODE(s) != OP_SETTABLE ||
      (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
       GET_OPCODE(mm) != OP_MMBINK))
    return 0;
  tmp = GETARG_A(i0);
  arr = GETARG_B(i0);
  key = for_a + 2;
  dst = GETARG_A(s);
  if (GETARG_C(i0) != key || GETARG_A(op) != tmp ||
      GETARG_B(op) != tmp || dst != arr || GETARG_B(s) != key ||
      GETARG_C(s) != tmp || GETARG_k(s) || tmp == key ||
      arr == for_a || arr == for_a + 1 || arr == for_a + 2 ||
      tmp == for_a || tmp == for_a + 1 || tmp == for_a + 2 ||
      luar_ffi_trace_array_kind(&b[arr]) != 2)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADDK) code = 0;
  else if (opc == OP_SUBK) code = 1;
  else if (opc == OP_MULK) code = 2;
  else if (opc == OP_DIVK) code = 3;
  else if (opc == OP_MODK) code = 4;
  else
    return 0;
  kidx = GETARG_C(op);
  if (kidx < 0 || kidx >= p->sizek ||
      (!ttisfloat(&p->k[kidx]) && !ttisinteger(&p->k[kidx])))
    return 0;
  return install_cdata_tint(p, J, backedge, 86, arr, code, tmp,
                            0, 0, kidx, for_a);
}


/* LUAR: `s.f = V` with a cdata scalar for an 8-byte field. */
static int match_cdata_field_fill_scalar (Proto *p, LuaR_JIT *J,
                                          ptrdiff_t start,
                                          ptrdiff_t backedge, int for_a,
                                          TValue *b) {
  ptrdiff_t pos = start;
  Instruction s, up;
  int enc, kidxstr, upidx, val_enc = 0;
  size_t off = 0, fsize = 0;
  TValue *cv, *fv;
  LClosure *cl;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    up = p->code[pos++];
    val_enc = -1 - GETARG_B(up);
    if (is_control_reg(GETARG_A(up), for_a))
      return 0;
  }
  if (pos + 1 != backedge)
    return 0;
  s = p->code[pos];
  if (GETARG_k(s) || (GET_OPCODE(s) != OP_SETFIELD &&
                      GET_OPCODE(s) != OP_SETTABUP))
    return 0;
  kidxstr = GETARG_B(s);
  if (kidxstr < 0 || kidxstr >= p->sizek ||
      !ttisstring(&p->k[kidxstr]))
    return 0;
  if (val_enc == 0) {
    val_enc = GETARG_C(s);
    if (is_control_reg(val_enc, for_a))
      return 0;
  }
  else if (GETARG_C(s) < 0)
    return 0;
  if (GET_OPCODE(s) == OP_SETFIELD) {
    enc = GETARG_A(s);
    if (is_control_reg(enc, for_a))
      return 0;
    cv = &b[enc];
  }
  else {
    upidx = GETARG_A(s);
    fv = &b[-1];
    if (!ttisLclosure(fv))
      return 0;
    cl = clLvalue(fv);
    if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
        cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
      return 0;
    enc = -1 - upidx;
    cv = cl->upvals[upidx]->v.p;
  }
  if (luar_ffi_trace_kind(cv) != 3 ||
      !luar_ffi_trace_field_int(cv, getstr(tsvalue(&p->k[kidxstr])),
                                &off, &fsize) || fsize == 0 ||
      off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 85, enc, val_enc, 0,
                            (int)off, 0, 0, for_a);
}


/* LUAR: `s.f = v` for a double field with a numeric scalar. */
static int match_cdata_field_fill_f64_scalar (Proto *p, LuaR_JIT *J,
                                              ptrdiff_t start,
                                              ptrdiff_t backedge,
                                              int for_a, TValue *b) {
  ptrdiff_t pos = start;
  Instruction s, up;
  int enc, kidxstr, upidx, val_enc = 0;
  size_t off = 0, fsize = 0;
  TValue *cv, *fv;
  LClosure *cl;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    up = p->code[pos++];
    val_enc = -1 - GETARG_B(up);
    if (is_control_reg(GETARG_A(up), for_a))
      return 0;
  }
  if (pos + 1 != backedge)
    return 0;
  s = p->code[pos];
  if (GETARG_k(s) || (GET_OPCODE(s) != OP_SETFIELD &&
                      GET_OPCODE(s) != OP_SETTABUP))
    return 0;
  kidxstr = GETARG_B(s);
  if (kidxstr < 0 || kidxstr >= p->sizek ||
      !ttisstring(&p->k[kidxstr]))
    return 0;
  if (val_enc == 0) {
    val_enc = GETARG_C(s);
    if (is_control_reg(val_enc, for_a))
      return 0;
  }
  else if (GETARG_C(s) < 0)
    return 0;
  if (GET_OPCODE(s) == OP_SETFIELD) {
    enc = GETARG_A(s);
    if (is_control_reg(enc, for_a))
      return 0;
    cv = &b[enc];
  }
  else {
    upidx = GETARG_A(s);
    fv = &b[-1];
    if (!ttisLclosure(fv))
      return 0;
    cl = clLvalue(fv);
    if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
        cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
      return 0;
    enc = -1 - upidx;
    cv = cl->upvals[upidx]->v.p;
  }
  if (luar_ffi_trace_kind(cv) != 3 ||
      !luar_ffi_trace_field_f64(cv, getstr(tsvalue(&p->k[kidxstr])),
                                &off, &fsize) || fsize != 8 ||
      off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 99, enc, val_enc, 0,
                            (int)off, 0, 0, for_a);
}


/* LUAR: `U.f = U.f OP k` for a double field. */
static int match_cdata_upval_field_tabup_f64_const (Proto *p, LuaR_JIT *J,
                                                    ptrdiff_t start,
                                                    ptrdiff_t backedge,
                                                    int for_a, TValue *b) {
  Instruction g, op, mm, set;
  int tmp, upidx, kidx, opc, code = -1;
  size_t off = 0, fsize = 0;
  TValue *fv, *uv;
  LClosure *cl;
  if (start + 4 != backedge)
    return 0;
  g = p->code[start];
  op = p->code[start + 1];
  mm = p->code[start + 2];
  set = p->code[start + 3];
  if (GET_OPCODE(g) != OP_GETTABUP || GET_OPCODE(set) != OP_SETTABUP ||
      (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
       GET_OPCODE(mm) != OP_MMBINK))
    return 0;
  tmp = GETARG_A(g);
  upidx = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      GETARG_A(set) != upidx || GETARG_B(set) != kidx ||
      GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp ||
      is_control_reg(tmp, for_a))
    return 0;
  fv = &b[-1];
  if (!ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
      cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
    return 0;
  uv = cl->upvals[upidx]->v.p;
  if (luar_ffi_trace_kind(uv) != 3 ||
      !luar_ffi_trace_field_f64(uv, getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize != 8 ||
      off > INT32_MAX)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADDK) code = 0;
  else if (opc == OP_SUBK) code = 1;
  else if (opc == OP_MULK) code = 2;
  else if (opc == OP_DIVK) code = 3;
  else if (opc == OP_MODK) code = 4;
  else
    return 0;
  {
    int idx = GETARG_C(op);
    if (idx < 0 || idx >= p->sizek ||
        (!ttisfloat(&p->k[idx]) && !ttisinteger(&p->k[idx])))
      return 0;
    return install_cdata_tint(p, J, backedge, 103, upidx, code, tmp,
                              (int)off, 0, idx, for_a);
  }
}


/* LUAR: `U.f = U.f OP v` for a double field with a numeric scalar. */
static int match_cdata_upval_field_tabup_f64_scalar (Proto *p, LuaR_JIT *J,
                                                     ptrdiff_t start,
                                                     ptrdiff_t backedge,
                                                     int for_a, TValue *b) {
  ptrdiff_t pos = start + 1;
  Instruction g, op, mm, set;
  int tmp, upidx, kidx, scalar_enc = 0, scalar_reg = -1, opc, code;
  size_t off = 0, fsize = 0;
  TValue *fv, *uv;
  LClosure *cl;
  if (backedge - start + 1 != 5 && backedge - start + 1 != 6)
    return 0;
  g = p->code[start];
  if (GET_OPCODE(g) != OP_GETTABUP)
    return 0;
  tmp = GETARG_A(g);
  upidx = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      is_control_reg(tmp, for_a))
    return 0;
  if (backedge - start + 1 == 6) {
    Instruction up = p->code[start + 1];
    if (GET_OPCODE(up) != OP_GETUPVAL)
      return 0;
    scalar_reg = GETARG_A(up);
    scalar_enc = -1 - GETARG_B(up);
    if (is_control_reg(scalar_reg, for_a))
      return 0;
    pos = start + 2;
  }
  if (pos + 3 != backedge)
    return 0;
  op = p->code[pos++];
  mm = p->code[pos++];
  set = p->code[pos++];
  if ((GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
       GET_OPCODE(mm) != OP_MMBINK) ||
      GET_OPCODE(set) != OP_SETTABUP || GETARG_A(set) != upidx ||
      GETARG_B(set) != kidx || GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp)
    return 0;
  if (scalar_enc == 0) {
    scalar_reg = GETARG_C(op);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == tmp)
      return 0;
    scalar_enc = scalar_reg;
  }
  else if (GETARG_C(op) != scalar_reg)
    return 0;
  fv = &b[-1];
  if (!ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
      cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
    return 0;
  uv = cl->upvals[upidx]->v.p;
  if (luar_ffi_trace_kind(uv) != 3 ||
      !luar_ffi_trace_field_f64(uv, getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize != 8 ||
      off > INT32_MAX)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_DIV) code = 3;
  else if (opc == OP_MOD) code = 4;
  else return 0;
  return install_cdata_tint(p, J, backedge, 102, upidx, code, tmp,
                            (int)off, scalar_enc, 0, for_a);
}


/* LUAR: `s.f = s.f OP v` for a double field with a numeric scalar. */
static int match_cdata_field_update_f64_scalar (Proto *p, LuaR_JIT *J,
                                                ptrdiff_t start,
                                                ptrdiff_t backedge,
                                                int for_a, TValue *b) {
  ptrdiff_t pos = start + 1;
  Instruction g, op, mm, set;
  int tmp, sreg, kidx, scalar_enc = 0, scalar_reg = -1, opc, code;
  size_t off = 0, fsize = 0;
  if (backedge - start + 1 != 5 && backedge - start + 1 != 6)
    return 0;
  g = p->code[start];
  if (GET_OPCODE(g) != OP_GETFIELD)
    return 0;
  tmp = GETARG_A(g);
  sreg = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      is_control_reg(tmp, for_a) || is_control_reg(sreg, for_a) ||
      luar_ffi_trace_kind(&b[sreg]) != 3 ||
      !luar_ffi_trace_field_f64(&b[sreg], getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize != 8 ||
      off > INT32_MAX)
    return 0;
  if (backedge - start + 1 == 6) {
    Instruction up = p->code[start + 1];
    if (GET_OPCODE(up) != OP_GETUPVAL)
      return 0;
    scalar_reg = GETARG_A(up);
    scalar_enc = -1 - GETARG_B(up);
    if (is_control_reg(scalar_reg, for_a))
      return 0;
    pos = start + 2;
  }
  if (pos + 3 != backedge)
    return 0;
  op = p->code[pos++];
  mm = p->code[pos++];
  set = p->code[pos++];
  if ((GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
       GET_OPCODE(mm) != OP_MMBINK) ||
      GET_OPCODE(set) != OP_SETFIELD || GETARG_A(set) != sreg ||
      GETARG_B(set) != kidx || GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp)
    return 0;
  if (scalar_enc == 0) {
    scalar_reg = GETARG_C(op);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == sreg ||
        scalar_reg == tmp)
      return 0;
    scalar_enc = scalar_reg;
  }
  else if (GETARG_C(op) != scalar_reg)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_DIV) code = 3;
  else if (opc == OP_MOD) code = 4;
  else return 0;
  return install_cdata_tint(p, J, backedge, 101, sreg, code, tmp,
                            (int)off, scalar_enc, 0, for_a);
}


/* LUAR: `s.f = s.f OP k` for a double field. */
static int match_cdata_field_update_f64_const (Proto *p, LuaR_JIT *J,
                                               ptrdiff_t start,
                                               ptrdiff_t backedge,
                                               int for_a, TValue *b) {
  Instruction g, op, mm, set;
  int tmp, sreg, kidx, opc, code = -1;
  size_t off = 0, fsize = 0;
  if (start + 4 != backedge)
    return 0;
  g = p->code[start];
  op = p->code[start + 1];
  mm = p->code[start + 2];
  set = p->code[start + 3];
  if (GET_OPCODE(g) != OP_GETFIELD || GET_OPCODE(set) != OP_SETFIELD ||
      (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
       GET_OPCODE(mm) != OP_MMBINK))
    return 0;
  tmp = GETARG_A(g);
  sreg = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      GETARG_A(set) != sreg || GETARG_B(set) != kidx ||
      GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp ||
      is_control_reg(tmp, for_a) || is_control_reg(sreg, for_a) ||
      luar_ffi_trace_kind(&b[sreg]) != 3 ||
      !luar_ffi_trace_field_f64(&b[sreg], getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize != 8 ||
      off > INT32_MAX)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADDK) code = 0;
  else if (opc == OP_SUBK) code = 1;
  else if (opc == OP_MULK) code = 2;
  else if (opc == OP_DIVK) code = 3;
  else if (opc == OP_MODK) code = 4;
  else
    return 0;
  {
    int idx = GETARG_C(op);
    if (idx < 0 || idx >= p->sizek ||
        (!ttisfloat(&p->k[idx]) && !ttisinteger(&p->k[idx])))
      return 0;
    return install_cdata_tint(p, J, backedge, 100, sreg, code, tmp,
                              (int)off, 0, idx, for_a);
  }
}


/* LUAR: `s.f = s.f OP v` for a float field with a numeric scalar. */
static int match_cdata_field_update_f32_scalar (Proto *p, LuaR_JIT *J,
                                                ptrdiff_t start,
                                                ptrdiff_t backedge,
                                                int for_a, TValue *b) {
  ptrdiff_t pos = start + 1;
  Instruction g, op, mm, set;
  int tmp, sreg, kidx, scalar_enc = 0, scalar_reg = -1, opc, code;
  size_t off = 0, fsize = 0;
  if (backedge - start + 1 != 5 && backedge - start + 1 != 6)
    return 0;
  g = p->code[start];
  if (GET_OPCODE(g) != OP_GETFIELD)
    return 0;
  tmp = GETARG_A(g);
  sreg = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      is_control_reg(tmp, for_a) || is_control_reg(sreg, for_a) ||
      luar_ffi_trace_kind(&b[sreg]) != 3 ||
      !luar_ffi_trace_field_f32(&b[sreg], getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize != 4 ||
      off > INT32_MAX)
    return 0;
  if (backedge - start + 1 == 6) {
    Instruction up = p->code[start + 1];
    if (GET_OPCODE(up) != OP_GETUPVAL)
      return 0;
    scalar_reg = GETARG_A(up);
    scalar_enc = -1 - GETARG_B(up);
    if (is_control_reg(scalar_reg, for_a))
      return 0;
    pos = start + 2;
  }
  if (pos + 3 != backedge)
    return 0;
  op = p->code[pos++];
  mm = p->code[pos++];
  set = p->code[pos++];
  if ((GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
       GET_OPCODE(mm) != OP_MMBINK) ||
      GET_OPCODE(set) != OP_SETFIELD || GETARG_A(set) != sreg ||
      GETARG_B(set) != kidx || GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp)
    return 0;
  if (scalar_enc == 0) {
    scalar_reg = GETARG_C(op);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == sreg ||
        scalar_reg == tmp)
      return 0;
    scalar_enc = scalar_reg;
  }
  else if (GETARG_C(op) != scalar_reg)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_DIV) code = 3;
  else if (opc == OP_MOD) code = 4;
  else return 0;
  return install_cdata_tint(p, J, backedge, 107, sreg, code, tmp,
                            (int)off, scalar_enc, 0, for_a);
}


/* LUAR: `s.f = s.f OP k` for a float field. */
static int match_cdata_field_update_f32_const (Proto *p, LuaR_JIT *J,
                                               ptrdiff_t start,
                                               ptrdiff_t backedge,
                                               int for_a, TValue *b) {
  Instruction g, op, mm, set;
  int tmp, sreg, kidx, opc, code = -1;
  size_t off = 0, fsize = 0;
  if (start + 4 != backedge)
    return 0;
  g = p->code[start];
  op = p->code[start + 1];
  mm = p->code[start + 2];
  set = p->code[start + 3];
  if (GET_OPCODE(g) != OP_GETFIELD || GET_OPCODE(set) != OP_SETFIELD ||
      (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
       GET_OPCODE(mm) != OP_MMBINK))
    return 0;
  tmp = GETARG_A(g);
  sreg = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      GETARG_A(set) != sreg || GETARG_B(set) != kidx ||
      GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp ||
      is_control_reg(tmp, for_a) || is_control_reg(sreg, for_a) ||
      luar_ffi_trace_kind(&b[sreg]) != 3 ||
      !luar_ffi_trace_field_f32(&b[sreg], getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize != 4 ||
      off > INT32_MAX)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADDK) code = 0;
  else if (opc == OP_SUBK) code = 1;
  else if (opc == OP_MULK) code = 2;
  else if (opc == OP_DIVK) code = 3;
  else if (opc == OP_MODK) code = 4;
  else
    return 0;
  {
    int idx = GETARG_C(op);
    if (idx < 0 || idx >= p->sizek ||
        (!ttisfloat(&p->k[idx]) && !ttisinteger(&p->k[idx])))
      return 0;
    return install_cdata_tint(p, J, backedge, 106, sreg, code, tmp,
                              (int)off, 0, idx, for_a);
  }
}


/* LUAR: `s.f = v` for a float field with a numeric scalar. */
static int match_cdata_field_fill_f32_scalar (Proto *p, LuaR_JIT *J,
                                              ptrdiff_t start,
                                              ptrdiff_t backedge,
                                              int for_a, TValue *b) {
  ptrdiff_t pos = start;
  Instruction s, up;
  int enc, kidxstr, upidx, val_enc = 0;
  size_t off = 0, fsize = 0;
  TValue *cv, *fv;
  LClosure *cl;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    up = p->code[pos++];
    val_enc = -1 - GETARG_B(up);
    if (is_control_reg(GETARG_A(up), for_a))
      return 0;
  }
  if (pos + 1 != backedge)
    return 0;
  s = p->code[pos];
  if (GETARG_k(s) || (GET_OPCODE(s) != OP_SETFIELD &&
                      GET_OPCODE(s) != OP_SETTABUP))
    return 0;
  kidxstr = GETARG_B(s);
  if (kidxstr < 0 || kidxstr >= p->sizek ||
      !ttisstring(&p->k[kidxstr]))
    return 0;
  if (val_enc == 0) {
    val_enc = GETARG_C(s);
    if (is_control_reg(val_enc, for_a))
      return 0;
  }
  else if (GETARG_C(s) < 0)
    return 0;
  if (GET_OPCODE(s) == OP_SETFIELD) {
    enc = GETARG_A(s);
    if (is_control_reg(enc, for_a))
      return 0;
    cv = &b[enc];
  }
  else {
    upidx = GETARG_A(s);
    fv = &b[-1];
    if (!ttisLclosure(fv))
      return 0;
    cl = clLvalue(fv);
    if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
        cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
      return 0;
    enc = -1 - upidx;
    cv = cl->upvals[upidx]->v.p;
  }
  if (luar_ffi_trace_kind(cv) != 3 ||
      !luar_ffi_trace_field_f32(cv, getstr(tsvalue(&p->k[kidxstr])),
                                &off, &fsize) || fsize != 4 ||
      off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 105, enc, val_enc, 0,
                            (int)off, 0, 0, for_a);
}


/* LUAR: `s.f = numeric constant` for a float field. */
static int match_cdata_field_fill_f32_const (Proto *p, LuaR_JIT *J,
                                             ptrdiff_t start,
                                             ptrdiff_t backedge,
                                             int for_a, TValue *b) {
  Instruction s;
  int enc, kidx, kidxstr, upidx;
  size_t off = 0, fsize = 0;
  TValue *cv, *fv;
  LClosure *cl;
  if (start + 1 != backedge)
    return 0;
  s = p->code[start];
  if (!GETARG_k(s) || GETARG_C(s) < 0 || GETARG_C(s) >= p->sizek ||
      (!ttisfloat(&p->k[GETARG_C(s)]) &&
       !ttisinteger(&p->k[GETARG_C(s)])))
    return 0;
  kidx = GETARG_C(s);
  kidxstr = GETARG_B(s);
  if (kidxstr < 0 || kidxstr >= p->sizek ||
      !ttisstring(&p->k[kidxstr]))
    return 0;
  if (GET_OPCODE(s) == OP_SETFIELD) {
    enc = GETARG_A(s);
    if (is_control_reg(enc, for_a))
      return 0;
    cv = &b[enc];
  }
  else if (GET_OPCODE(s) == OP_SETTABUP) {
    upidx = GETARG_A(s);
    fv = &b[-1];
    if (!ttisLclosure(fv))
      return 0;
    cl = clLvalue(fv);
    if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
        cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
      return 0;
    enc = -1 - upidx;
    cv = cl->upvals[upidx]->v.p;
  }
  else
    return 0;
  if (luar_ffi_trace_kind(cv) != 3 ||
      !luar_ffi_trace_field_f32(cv, getstr(tsvalue(&p->k[kidxstr])),
                                &off, &fsize) || fsize != 4 ||
      off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 104, enc, -2, 0,
                            (int)off, 0, kidx, for_a);
}


/* LUAR: `s.f = numeric constant` for a double field. */
static int match_cdata_field_fill_f64_const (Proto *p, LuaR_JIT *J,
                                             ptrdiff_t start,
                                             ptrdiff_t backedge,
                                             int for_a, TValue *b) {
  Instruction s;
  int enc, kidx, kidxstr, upidx;
  size_t off = 0, fsize = 0;
  TValue *cv, *fv;
  LClosure *cl;
  if (start + 1 != backedge)
    return 0;
  s = p->code[start];
  if (!GETARG_k(s) || GETARG_C(s) < 0 || GETARG_C(s) >= p->sizek ||
      (!ttisfloat(&p->k[GETARG_C(s)]) &&
       !ttisinteger(&p->k[GETARG_C(s)])))
    return 0;
  kidx = GETARG_C(s);
  kidxstr = GETARG_B(s);
  if (kidxstr < 0 || kidxstr >= p->sizek ||
      !ttisstring(&p->k[kidxstr]))
    return 0;
  if (GET_OPCODE(s) == OP_SETFIELD) {
    enc = GETARG_A(s);
    if (is_control_reg(enc, for_a))
      return 0;
    cv = &b[enc];
  }
  else if (GET_OPCODE(s) == OP_SETTABUP) {
    upidx = GETARG_A(s);
    fv = &b[-1];
    if (!ttisLclosure(fv))
      return 0;
    cl = clLvalue(fv);
    if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
        cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
      return 0;
    enc = -1 - upidx;
    cv = cl->upvals[upidx]->v.p;
  }
  else
    return 0;
  if (luar_ffi_trace_kind(cv) != 3 ||
      !luar_ffi_trace_field_f64(cv, getstr(tsvalue(&p->k[kidxstr])),
                                &off, &fsize) || fsize != 8 ||
      off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 98, enc, -2, 0,
                            (int)off, 0, kidx, for_a);
}


/* LUAR: `s.f = integer constant` for an 8-byte field. */
static int match_cdata_field_fill_const (Proto *p, LuaR_JIT *J,
                                         ptrdiff_t start, ptrdiff_t backedge,
                                         int for_a, TValue *b) {
  Instruction s;
  int enc, kidx, kidxstr, upidx;
  size_t off = 0, fsize = 0;
  TValue *cv, *fv;
  LClosure *cl;
  if (start + 1 != backedge)
    return 0;
  s = p->code[start];
  if (!GETARG_k(s) || GETARG_C(s) < 0 || GETARG_C(s) >= p->sizek ||
      (!ttisinteger(&p->k[GETARG_C(s)]) &&
       !ttisboolean(&p->k[GETARG_C(s)])))
    return 0;
  kidx = GETARG_C(s);
  kidxstr = GETARG_B(s);
  if (kidxstr < 0 || kidxstr >= p->sizek ||
      !ttisstring(&p->k[kidxstr]))
    return 0;
  if (GET_OPCODE(s) == OP_SETFIELD) {
    enc = GETARG_A(s);
    if (is_control_reg(enc, for_a))
      return 0;
    cv = &b[enc];
  }
  else if (GET_OPCODE(s) == OP_SETTABUP) {
    upidx = GETARG_A(s);
    fv = &b[-1];
    if (!ttisLclosure(fv))
      return 0;
    cl = clLvalue(fv);
    if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
        cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
      return 0;
    enc = -1 - upidx;
    cv = cl->upvals[upidx]->v.p;
  }
  else
    return 0;
  if (luar_ffi_trace_kind(cv) != 3 ||
      !luar_ffi_trace_field_int(cv, getstr(tsvalue(&p->k[kidxstr])),
                                &off, &fsize) || fsize == 0 ||
      off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 84, enc, -2, 0,
                            (int)off, 0, kidx, for_a);
}


/* LUAR: `dst.f = src.f` where either struct is a closure upvalue. */
static int match_cdata_field_copy_upval (Proto *p, LuaR_JIT *J,
                                         ptrdiff_t start,
                                         ptrdiff_t backedge, int for_a,
                                         TValue *b) {
  Instruction g, s;
  int tmp, src_enc, dst_enc, sk, dk, src_is_up, dst_is_up;
  size_t soff = 0, doff = 0, ssize = 0, dsize = 0;
  TValue *sv, *dv;
  if (start + 2 != backedge)
    return 0;
  g = p->code[start];
  s = p->code[start + 1];
  tmp = GETARG_A(g);
  if (GET_OPCODE(g) == OP_GETTABUP) {
    src_is_up = 1;
    src_enc = -1 - GETARG_B(g);
    sk = GETARG_C(g);
  }
  else if (GET_OPCODE(g) == OP_GETFIELD) {
    src_is_up = 0;
    src_enc = GETARG_B(g);
    sk = GETARG_C(g);
  }
  else
    return 0;
  if (GET_OPCODE(s) == OP_SETTABUP) {
    dst_is_up = 1;
    dst_enc = -1 - GETARG_A(s);
    dk = GETARG_B(s);
  }
  else if (GET_OPCODE(s) == OP_SETFIELD) {
    dst_is_up = 0;
    dst_enc = GETARG_A(s);
    dk = GETARG_B(s);
  }
  else
    return 0;
  if ((!src_is_up && !dst_is_up) || GETARG_C(s) != tmp ||
      GETARG_k(s) || sk < 0 || sk >= p->sizek ||
      dk < 0 || dk >= p->sizek || !ttisstring(&p->k[sk]) ||
      !ttisstring(&p->k[dk]) || is_control_reg(tmp, for_a) ||
      (!src_is_up && is_control_reg(src_enc, for_a)) ||
      (!dst_is_up && is_control_reg(dst_enc, for_a)))
    return 0;
  sv = pair_source_value((TValue *)b, (src_enc >= 0) ? src_enc : -1,
                         (src_enc >= 0) ? -1 : (-1 - src_enc));
  dv = pair_source_value((TValue *)b, (dst_enc >= 0) ? dst_enc : -1,
                         (dst_enc >= 0) ? -1 : (-1 - dst_enc));
  if (sv == NULL || dv == NULL || luar_ffi_trace_kind(sv) != 3 ||
      luar_ffi_trace_kind(dv) != 3 ||
      !luar_ffi_trace_field_int(sv, getstr(tsvalue(&p->k[sk])),
                                &soff, &ssize) || ssize == 0 ||
      !luar_ffi_trace_field_int(dv, getstr(tsvalue(&p->k[dk])),
                                &doff, &dsize) || dsize == 0 ||
      soff > INT32_MAX || doff > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 83, dst_enc, src_enc, tmp,
                            (int)doff, (int)soff, 0, for_a);
}


/* LUAR: `dst.f = src.f` for double fields with an upvalue struct. */
static int match_cdata_field_copy_f32_upval (Proto *p, LuaR_JIT *J,
                                             ptrdiff_t start,
                                             ptrdiff_t backedge,
                                             int for_a, TValue *b) {
  Instruction g, s;
  int tmp, src_enc, dst_enc, sk, dk, src_is_up, dst_is_up;
  size_t soff = 0, doff = 0, ssize = 0, dsize = 0;
  TValue *sv, *dv;
  if (start + 2 != backedge)
    return 0;
  g = p->code[start];
  s = p->code[start + 1];
  tmp = GETARG_A(g);
  if (GET_OPCODE(g) == OP_GETTABUP) {
    src_is_up = 1;
    src_enc = -1 - GETARG_B(g);
    sk = GETARG_C(g);
  }
  else if (GET_OPCODE(g) == OP_GETFIELD) {
    src_is_up = 0;
    src_enc = GETARG_B(g);
    sk = GETARG_C(g);
  }
  else
    return 0;
  if (GET_OPCODE(s) == OP_SETTABUP) {
    dst_is_up = 1;
    dst_enc = -1 - GETARG_A(s);
    dk = GETARG_B(s);
  }
  else if (GET_OPCODE(s) == OP_SETFIELD) {
    dst_is_up = 0;
    dst_enc = GETARG_A(s);
    dk = GETARG_B(s);
  }
  else
    return 0;
  if ((!src_is_up && !dst_is_up) || GETARG_C(s) != tmp ||
      GETARG_k(s) || sk < 0 || sk >= p->sizek ||
      dk < 0 || dk >= p->sizek || !ttisstring(&p->k[sk]) ||
      !ttisstring(&p->k[dk]) || is_control_reg(tmp, for_a) ||
      (!src_is_up && is_control_reg(src_enc, for_a)) ||
      (!dst_is_up && is_control_reg(dst_enc, for_a)))
    return 0;
  sv = pair_source_value((TValue *)b, (src_enc >= 0) ? src_enc : -1,
                         (src_enc >= 0) ? -1 : (-1 - src_enc));
  dv = pair_source_value((TValue *)b, (dst_enc >= 0) ? dst_enc : -1,
                         (dst_enc >= 0) ? -1 : (-1 - dst_enc));
  if (sv == NULL || dv == NULL || luar_ffi_trace_kind(sv) != 3 ||
      luar_ffi_trace_kind(dv) != 3 ||
      !luar_ffi_trace_field_f32(sv, getstr(tsvalue(&p->k[sk])),
                                &soff, &ssize) || ssize != 4 ||
      !luar_ffi_trace_field_f32(dv, getstr(tsvalue(&p->k[dk])),
                                &doff, &dsize) || dsize != 4 ||
      soff > INT32_MAX || doff > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 83, dst_enc, src_enc, tmp,
                            (int)doff, (int)soff, 0, for_a);
}


static int match_cdata_field_copy_f64_upval (Proto *p, LuaR_JIT *J,
                                             ptrdiff_t start,
                                             ptrdiff_t backedge,
                                             int for_a, TValue *b) {
  Instruction g, s;
  int tmp, src_enc, dst_enc, sk, dk, src_is_up, dst_is_up;
  size_t soff = 0, doff = 0, ssize = 0, dsize = 0;
  TValue *sv, *dv;
  if (start + 2 != backedge)
    return 0;
  g = p->code[start];
  s = p->code[start + 1];
  tmp = GETARG_A(g);
  if (GET_OPCODE(g) == OP_GETTABUP) {
    src_is_up = 1;
    src_enc = -1 - GETARG_B(g);
    sk = GETARG_C(g);
  }
  else if (GET_OPCODE(g) == OP_GETFIELD) {
    src_is_up = 0;
    src_enc = GETARG_B(g);
    sk = GETARG_C(g);
  }
  else
    return 0;
  if (GET_OPCODE(s) == OP_SETTABUP) {
    dst_is_up = 1;
    dst_enc = -1 - GETARG_A(s);
    dk = GETARG_B(s);
  }
  else if (GET_OPCODE(s) == OP_SETFIELD) {
    dst_is_up = 0;
    dst_enc = GETARG_A(s);
    dk = GETARG_B(s);
  }
  else
    return 0;
  if ((!src_is_up && !dst_is_up) || GETARG_C(s) != tmp ||
      GETARG_k(s) || sk < 0 || sk >= p->sizek ||
      dk < 0 || dk >= p->sizek || !ttisstring(&p->k[sk]) ||
      !ttisstring(&p->k[dk]) || is_control_reg(tmp, for_a) ||
      (!src_is_up && is_control_reg(src_enc, for_a)) ||
      (!dst_is_up && is_control_reg(dst_enc, for_a)))
    return 0;
  sv = pair_source_value((TValue *)b, (src_enc >= 0) ? src_enc : -1,
                         (src_enc >= 0) ? -1 : (-1 - src_enc));
  dv = pair_source_value((TValue *)b, (dst_enc >= 0) ? dst_enc : -1,
                         (dst_enc >= 0) ? -1 : (-1 - dst_enc));
  if (sv == NULL || dv == NULL || luar_ffi_trace_kind(sv) != 3 ||
      luar_ffi_trace_kind(dv) != 3 ||
      !luar_ffi_trace_field_f64(sv, getstr(tsvalue(&p->k[sk])),
                                &soff, &ssize) || ssize != 8 ||
      !luar_ffi_trace_field_f64(dv, getstr(tsvalue(&p->k[dk])),
                                &doff, &dsize) || dsize != 8 ||
      soff > INT32_MAX || doff > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 83, dst_enc, src_enc, tmp,
                            (int)doff, (int)soff, 0, for_a);
}


/* LUAR: `dst.f = src.f` between float and double fields. */
static int match_cdata_field_copy_float_mixed (Proto *p, LuaR_JIT *J,
                                               ptrdiff_t start,
                                               ptrdiff_t backedge,
                                               int for_a, TValue *b) {
  Instruction g, s;
  int tmp, src, dst, sk, dk;
  size_t soff = 0, doff = 0, ssize = 0, dsize = 0;
  int sf32, df32, sf64, df64;
  if (start + 2 != backedge)
    return 0;
  g = p->code[start];
  s = p->code[start + 1];
  if (GET_OPCODE(g) != OP_GETFIELD || GET_OPCODE(s) != OP_SETFIELD ||
      GETARG_k(s))
    return 0;
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  sk = GETARG_C(g);
  dst = GETARG_A(s);
  dk = GETARG_B(s);
  if (GETARG_C(s) != tmp || sk < 0 || sk >= p->sizek ||
      dk < 0 || dk >= p->sizek || !ttisstring(&p->k[sk]) ||
      !ttisstring(&p->k[dk]) || is_control_reg(tmp, for_a) ||
      is_control_reg(src, for_a) || is_control_reg(dst, for_a) ||
      luar_ffi_trace_kind(&b[src]) != 3 ||
      luar_ffi_trace_kind(&b[dst]) != 3)
    return 0;
  sf32 = luar_ffi_trace_field_f32(&b[src], getstr(tsvalue(&p->k[sk])),
                                  &soff, &ssize);
  sf64 = !sf32 && luar_ffi_trace_field_f64(&b[src],
                    getstr(tsvalue(&p->k[sk])), &soff, &ssize);
  df32 = luar_ffi_trace_field_f32(&b[dst], getstr(tsvalue(&p->k[dk])),
                                  &doff, &dsize);
  df64 = !df32 && luar_ffi_trace_field_f64(&b[dst],
                    getstr(tsvalue(&p->k[dk])), &doff, &dsize);
  if ((!sf32 && !sf64) || (!df32 && !df64) ||
      (sf32 && df32) || (sf64 && df64) ||
      soff > INT32_MAX || doff > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 108, dst, src, tmp,
                            (int)doff, (int)soff, 0, for_a);
}


/* LUAR: `dst.f = src.f` for float struct fields. */
static int match_cdata_field_copy_f32 (Proto *p, LuaR_JIT *J,
                                       ptrdiff_t start, ptrdiff_t backedge,
                                       int for_a, TValue *b) {
  Instruction g, s;
  int tmp, src, dst, sk, dk;
  size_t soff = 0, doff = 0, ssize = 0, dsize = 0;
  if (start + 2 != backedge)
    return 0;
  g = p->code[start];
  s = p->code[start + 1];
  if (GET_OPCODE(g) != OP_GETFIELD || GET_OPCODE(s) != OP_SETFIELD ||
      GETARG_k(s))
    return 0;
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  sk = GETARG_C(g);
  dst = GETARG_A(s);
  dk = GETARG_B(s);
  if (GETARG_C(s) != tmp || sk < 0 || sk >= p->sizek ||
      dk < 0 || dk >= p->sizek || !ttisstring(&p->k[sk]) ||
      !ttisstring(&p->k[dk]) || is_control_reg(tmp, for_a) ||
      is_control_reg(src, for_a) || is_control_reg(dst, for_a) ||
      luar_ffi_trace_kind(&b[src]) != 3 ||
      luar_ffi_trace_kind(&b[dst]) != 3 ||
      !luar_ffi_trace_field_f32(&b[src], getstr(tsvalue(&p->k[sk])),
                                &soff, &ssize) || ssize != 4 ||
      !luar_ffi_trace_field_f32(&b[dst], getstr(tsvalue(&p->k[dk])),
                                &doff, &dsize) || dsize != 4 ||
      soff > INT32_MAX || doff > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 82, dst, src, tmp,
                            (int)doff, (int)soff, 0, for_a);
}


/* LUAR: `dst.f = src.f` for 8-byte double struct fields. */
static int match_cdata_field_copy_f64 (Proto *p, LuaR_JIT *J,
                                       ptrdiff_t start, ptrdiff_t backedge,
                                       int for_a, TValue *b) {
  Instruction g, s;
  int tmp, src, dst, sk, dk;
  size_t soff = 0, doff = 0, ssize = 0, dsize = 0;
  if (start + 2 != backedge)
    return 0;
  g = p->code[start];
  s = p->code[start + 1];
  if (GET_OPCODE(g) != OP_GETFIELD || GET_OPCODE(s) != OP_SETFIELD ||
      GETARG_k(s))
    return 0;
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  sk = GETARG_C(g);
  dst = GETARG_A(s);
  dk = GETARG_B(s);
  if (GETARG_C(s) != tmp || sk < 0 || sk >= p->sizek ||
      dk < 0 || dk >= p->sizek || !ttisstring(&p->k[sk]) ||
      !ttisstring(&p->k[dk]) || is_control_reg(tmp, for_a) ||
      is_control_reg(src, for_a) || is_control_reg(dst, for_a) ||
      luar_ffi_trace_kind(&b[src]) != 3 ||
      luar_ffi_trace_kind(&b[dst]) != 3 ||
      !luar_ffi_trace_field_f64(&b[src], getstr(tsvalue(&p->k[sk])),
                                &soff, &ssize) || ssize != 8 ||
      !luar_ffi_trace_field_f64(&b[dst], getstr(tsvalue(&p->k[dk])),
                                &doff, &dsize) || dsize != 8 ||
      soff > INT32_MAX || doff > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 82, dst, src, tmp,
                            (int)doff, (int)soff, 0, for_a);
}


/* LUAR: `dst.f = src.f` for 8-byte integer struct fields. */
static int match_cdata_field_copy (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t start, ptrdiff_t backedge,
                                   int for_a, TValue *b) {
  Instruction g, s;
  int tmp, src, dst, sk, dk;
  size_t soff = 0, doff = 0, ssize = 0, dsize = 0;
  if (start + 2 != backedge)
    return 0;
  g = p->code[start];
  s = p->code[start + 1];
  if (GET_OPCODE(g) != OP_GETFIELD || GET_OPCODE(s) != OP_SETFIELD ||
      GETARG_k(s))
    return 0;
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  sk = GETARG_C(g);
  dst = GETARG_A(s);
  dk = GETARG_B(s);
  if (GETARG_C(s) != tmp || sk < 0 || sk >= p->sizek ||
      dk < 0 || dk >= p->sizek || !ttisstring(&p->k[sk]) ||
      !ttisstring(&p->k[dk]) || is_control_reg(tmp, for_a) ||
      is_control_reg(src, for_a) || is_control_reg(dst, for_a) ||
      luar_ffi_trace_kind(&b[src]) != 3 ||
      luar_ffi_trace_kind(&b[dst]) != 3 ||
      !luar_ffi_trace_field_int(&b[src], getstr(tsvalue(&p->k[sk])),
                                &soff, &ssize) || ssize == 0 ||
      !luar_ffi_trace_field_int(&b[dst], getstr(tsvalue(&p->k[dk])),
                                &doff, &dsize) || dsize == 0 ||
      soff > INT32_MAX || doff > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 82, dst, src, tmp,
                            (int)doff, (int)soff, 0, for_a);
}


/* LUAR: `acc = acc OP s.f` for an 8-byte integer field. */
static int match_cdata_field_sum (Proto *p, LuaR_JIT *J,
                                  ptrdiff_t start, ptrdiff_t backedge,
                                  int for_a, TValue *b) {
  Instruction g, op;
  int tmp, sreg, kidx, acc, opc, code;
  size_t off = 0, fsize = 0;
  if (start + 3 != backedge)
    return 0;
  g = p->code[start];
  op = p->code[start + 1];
  if (GET_OPCODE(g) != OP_GETFIELD ||
      GET_OPCODE(p->code[start + 2]) != OP_MMBIN ||
      (GET_OPCODE(op) != OP_ADD && GET_OPCODE(op) != OP_SUB &&
       GET_OPCODE(op) != OP_MUL && GET_OPCODE(op) != OP_BAND &&
       GET_OPCODE(op) != OP_BOR && GET_OPCODE(op) != OP_BXOR &&
       GET_OPCODE(op) != OP_SHL && GET_OPCODE(op) != OP_SHR &&
       GET_OPCODE(op) != OP_IDIV && GET_OPCODE(op) != OP_MOD))
    return 0;
  tmp = GETARG_A(g);
  sreg = GETARG_B(g);
  kidx = GETARG_C(g);
  acc = GETARG_A(op);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      GETARG_B(op) != acc || GETARG_C(op) != tmp ||
      tmp == acc || is_control_reg(tmp, for_a) ||
      is_control_reg(sreg, for_a) || is_control_reg(acc, for_a) ||
      luar_ffi_trace_kind(&b[sreg]) != 3 ||
      !luar_ffi_trace_field_i64(&b[sreg], getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize != 8)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_BAND) code = 6;
  else if (opc == OP_BOR) code = 7;
  else if (opc == OP_BXOR) code = 8;
  else if (opc == OP_SHL) code = 9;
  else if (opc == OP_SHR) code = 10;
  else if (opc == OP_IDIV) code = 3;
  else code = 4;
  if (off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 80, sreg, code, acc,
                            (int)off, tmp, 0, for_a);
}


/* LUAR: `acc = acc OP U.f` for an upvalue struct field. */
static int match_cdata_upval_field_sum (Proto *p, LuaR_JIT *J,
                                        ptrdiff_t start, ptrdiff_t backedge,
                                        int for_a, TValue *b) {
  Instruction g, op;
  int tmp, upidx, kidx, acc, opc, code;
  size_t off = 0, fsize = 0;
  TValue *fv, *uv;
  LClosure *cl;
  if (start + 3 != backedge)
    return 0;
  g = p->code[start];
  op = p->code[start + 1];
  if (GET_OPCODE(g) != OP_GETTABUP ||
      GET_OPCODE(p->code[start + 2]) != OP_MMBIN ||
      (GET_OPCODE(op) != OP_ADD && GET_OPCODE(op) != OP_SUB &&
       GET_OPCODE(op) != OP_MUL && GET_OPCODE(op) != OP_BAND &&
       GET_OPCODE(op) != OP_BOR && GET_OPCODE(op) != OP_BXOR &&
       GET_OPCODE(op) != OP_SHL && GET_OPCODE(op) != OP_SHR &&
       GET_OPCODE(op) != OP_IDIV && GET_OPCODE(op) != OP_MOD))
    return 0;
  tmp = GETARG_A(g);
  upidx = GETARG_B(g);
  kidx = GETARG_C(g);
  acc = GETARG_A(op);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      GETARG_B(op) != acc || GETARG_C(op) != tmp ||
      tmp == acc || is_control_reg(tmp, for_a) ||
      is_control_reg(acc, for_a))
    return 0;
  fv = &b[-1];
  if (!ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
      cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
    return 0;
  uv = cl->upvals[upidx]->v.p;
  if (luar_ffi_trace_kind(uv) != 3 ||
      !luar_ffi_trace_field_i64(uv, getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize != 8)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_BAND) code = 6;
  else if (opc == OP_BOR) code = 7;
  else if (opc == OP_BXOR) code = 8;
  else if (opc == OP_SHL) code = 9;
  else if (opc == OP_SHR) code = 10;
  else if (opc == OP_IDIV) code = 3;
  else code = 4;
  if (off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 81, upidx, code, acc,
                            (int)off, tmp, 0, for_a);
}


/* LUAR: `s.f = s.f OP V` with a cdata scalar for an 8-byte field. */
static int match_cdata_field_update_scalar (Proto *p, LuaR_JIT *J,
                                            ptrdiff_t start,
                                            ptrdiff_t backedge, int for_a,
                                            TValue *b) {
  ptrdiff_t pos = start + 1;
  Instruction g, op, set;
  int tmp, sreg, kidx, opc, code = -1, scalar_enc = 0, scalar_reg = -1;
  size_t off = 0, fsize = 0;
  if (start + 4 != backedge && start + 5 != backedge)
    return 0;
  g = p->code[start];
  if (GET_OPCODE(g) != OP_GETFIELD)
    return 0;
  tmp = GETARG_A(g);
  sreg = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      is_control_reg(tmp, for_a) || is_control_reg(sreg, for_a) ||
      luar_ffi_trace_kind(&b[sreg]) != 3 ||
      !luar_ffi_trace_field_int(&b[sreg], getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) ||
      fsize == 0)
    return 0;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    scalar_reg = GETARG_A(up);
    scalar_enc = -1 - GETARG_B(up);
    if (is_control_reg(scalar_reg, for_a))
      return 0;
  }
  if (pos + 3 != backedge)
    return 0;
  op = p->code[pos++];
  {
    Instruction mm = p->code[pos++];
    if (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
        GET_OPCODE(mm) != OP_MMBINK)
      return 0;
  }
  set = p->code[pos++];
  if (GET_OPCODE(set) != OP_SETFIELD || GETARG_A(set) != sreg ||
      GETARG_B(set) != kidx || GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp)
    return 0;
  if (scalar_enc == 0) {
    scalar_reg = GETARG_C(op);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == sreg ||
        scalar_reg == tmp)
      return 0;
    scalar_enc = scalar_reg;
  }
  else if (GETARG_C(op) != scalar_reg)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_IDIV) code = 3;
  else if (opc == OP_MOD) code = 4;
  else if (opc == OP_BAND) code = 6;
  else if (opc == OP_BOR) code = 7;
  else if (opc == OP_BXOR) code = 8;
  else if (opc == OP_SHL) code = 9;
  else if (opc == OP_SHR) code = 10;
  else return 0;
  if (off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 77, sreg, code, tmp,
                            (int)off, scalar_enc, 0, for_a);
}


/* LUAR: `U.f = U.f OP V` with a cdata scalar through GETTABUP/SETTABUP. */
static int match_cdata_upval_field_tabup_scalar (Proto *p, LuaR_JIT *J,
                                                 ptrdiff_t start,
                                                 ptrdiff_t backedge,
                                                 int for_a, TValue *b) {
  ptrdiff_t pos = start + 1;
  Instruction g, op, set;
  int tmp, upidx, kidx, opc, code = -1, scalar_enc = 0, scalar_reg = -1;
  size_t off = 0, fsize = 0;
  TValue *fv, *uv;
  LClosure *cl;
  if (start + 4 != backedge && start + 5 != backedge)
    return 0;
  g = p->code[start];
  if (GET_OPCODE(g) != OP_GETTABUP)
    return 0;
  tmp = GETARG_A(g);
  upidx = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      is_control_reg(tmp, for_a))
    return 0;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    scalar_reg = GETARG_A(up);
    scalar_enc = -1 - GETARG_B(up);
    if (is_control_reg(scalar_reg, for_a))
      return 0;
  }
  if (pos + 3 != backedge)
    return 0;
  op = p->code[pos++];
  {
    Instruction mm = p->code[pos++];
    if (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
        GET_OPCODE(mm) != OP_MMBINK)
      return 0;
  }
  set = p->code[pos++];
  if (GET_OPCODE(set) != OP_SETTABUP || GETARG_A(set) != upidx ||
      GETARG_B(set) != kidx || GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp)
    return 0;
  if (scalar_enc == 0) {
    scalar_reg = GETARG_C(op);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == tmp)
      return 0;
    scalar_enc = scalar_reg;
  }
  else if (GETARG_C(op) != scalar_reg)
    return 0;
  fv = &b[-1];
  if (!ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
      cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
    return 0;
  uv = cl->upvals[upidx]->v.p;
  if (luar_ffi_trace_kind(uv) != 3 ||
      !luar_ffi_trace_field_int(uv, getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize == 0)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_IDIV) code = 3;
  else if (opc == OP_MOD) code = 4;
  else if (opc == OP_BAND) code = 6;
  else if (opc == OP_BOR) code = 7;
  else if (opc == OP_BXOR) code = 8;
  else if (opc == OP_SHL) code = 9;
  else if (opc == OP_SHR) code = 10;
  else return 0;
  if (off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 79, upidx, code, tmp,
                            (int)off, scalar_enc, 0, for_a);
}


/* LUAR: `U.f = U.f OP k` compiled as GETTABUP/SETTABUP. */
static int match_cdata_upval_field_tabup_const (Proto *p, LuaR_JIT *J,
                                                ptrdiff_t start,
                                                ptrdiff_t backedge,
                                                int for_a, TValue *b) {
  Instruction g, op, set;
  int tmp, upidx, kidx, opc, code = -1;
  int64_t scalar = 0;
  size_t off = 0, fsize = 0;
  TValue *fv, *uv;
  LClosure *cl;
  if (start + 4 != backedge)
    return 0;
  g = p->code[start];
  op = p->code[start + 1];
  set = p->code[start + 3];
  if (GET_OPCODE(g) != OP_GETTABUP || GET_OPCODE(set) != OP_SETTABUP ||
      (GET_OPCODE(p->code[start + 2]) != OP_MMBINI &&
       GET_OPCODE(p->code[start + 2]) != OP_MMBINK &&
       GET_OPCODE(p->code[start + 2]) != OP_MMBIN))
    return 0;
  tmp = GETARG_A(g);
  upidx = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      GETARG_A(set) != upidx || GETARG_B(set) != kidx ||
      GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp ||
      is_control_reg(tmp, for_a))
    return 0;
  fv = &b[-1];
  if (!ttisLclosure(fv))
    return 0;
  cl = clLvalue(fv);
  if (upidx < 0 || upidx >= cl->p->sizeupvalues ||
      cl->upvals[upidx] == NULL || cl->upvals[upidx]->v.p == NULL)
    return 0;
  uv = cl->upvals[upidx]->v.p;
  if (luar_ffi_trace_kind(uv) != 3 ||
      !luar_ffi_trace_field_int(uv, getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize == 0)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADDI) {
    code = 0; scalar = GETARG_sC(op);
  }
  else if (opc == OP_SHLI) {
    code = 9; scalar = GETARG_sC(op);
  }
  else if (opc == OP_SHRI) {
    int64_t n = GETARG_sC(op);
    code = (n < 0) ? 9 : 10;
    scalar = (n < 0) ? -n : n;
  }
  else if (opc == OP_ADDK || opc == OP_SUBK || opc == OP_MULK ||
           opc == OP_BANDK || opc == OP_BORK || opc == OP_BXORK ||
           opc == OP_IDIVK || opc == OP_MODK) {
    int idx = GETARG_C(op);
    if (idx < 0 || idx >= p->sizek || !ttisinteger(&p->k[idx]))
      return 0;
    scalar = ivalue(&p->k[idx]);
    code = (opc == OP_ADDK) ? 0 : (opc == OP_SUBK) ? 1 :
           (opc == OP_MULK) ? 2 : (opc == OP_IDIVK) ? 3 :
           (opc == OP_MODK) ? 4 : (opc == OP_BANDK) ? 6 :
           (opc == OP_BORK) ? 7 : 8;
  }
  else
    return 0;
  if (scalar < INT32_MIN || scalar > INT32_MAX || off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 78, upidx, code, tmp,
                            (int)off, 0, (int)scalar, for_a);
}


/* LUAR: `U.f = U.f OP k` for an upvalue struct field. */
static int match_cdata_upval_field_update_const (Proto *p, LuaR_JIT *J,
                                                 ptrdiff_t start,
                                                 ptrdiff_t backedge,
                                                 int for_a, TValue *b) {
  Instruction up, g, op, set;
  int sreg, tmp, kidx, opc, code = -1;
  int64_t scalar = 0;
  size_t off = 0, fsize = 0;
  if (start + 5 != backedge)
    return 0;
  up = p->code[start];
  g = p->code[start + 1];
  op = p->code[start + 2];
  set = p->code[start + 4];
  if (GET_OPCODE(up) != OP_GETUPVAL || GET_OPCODE(g) != OP_GETFIELD ||
      GET_OPCODE(set) != OP_SETFIELD ||
      (GET_OPCODE(p->code[start + 3]) != OP_MMBINI &&
       GET_OPCODE(p->code[start + 3]) != OP_MMBINK &&
       GET_OPCODE(p->code[start + 3]) != OP_MMBIN))
    return 0;
  sreg = GETARG_A(up);
  tmp = GETARG_A(g);
  kidx = GETARG_C(g);
  if (GETARG_B(g) != sreg || kidx < 0 || kidx >= p->sizek ||
      !ttisstring(&p->k[kidx]) ||
      GETARG_A(set) != sreg || GETARG_B(set) != kidx ||
      GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp ||
      is_control_reg(sreg, for_a) || is_control_reg(tmp, for_a) ||
      !luar_ffi_trace_field_int(&b[sreg], getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) || fsize == 0)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADDI) {
    code = 0; scalar = GETARG_sC(op);
  }
  else if (opc == OP_SHLI) {
    code = 9; scalar = GETARG_sC(op);
  }
  else if (opc == OP_SHRI) {
    int64_t n = GETARG_sC(op);
    code = (n < 0) ? 9 : 10;
    scalar = (n < 0) ? -n : n;
  }
  else if (opc == OP_ADDK || opc == OP_SUBK || opc == OP_MULK ||
           opc == OP_BANDK || opc == OP_BORK || opc == OP_BXORK ||
           opc == OP_IDIVK || opc == OP_MODK) {
    int idx = GETARG_C(op);
    if (idx < 0 || idx >= p->sizek || !ttisinteger(&p->k[idx]))
      return 0;
    scalar = ivalue(&p->k[idx]);
    code = (opc == OP_ADDK) ? 0 : (opc == OP_SUBK) ? 1 :
           (opc == OP_MULK) ? 2 : (opc == OP_IDIVK) ? 3 :
           (opc == OP_MODK) ? 4 : (opc == OP_BANDK) ? 6 :
           (opc == OP_BORK) ? 7 : 8;
  }
  else
    return 0;
  if (scalar < INT32_MIN || scalar > INT32_MAX || off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 76, sreg, code, tmp,
                            (int)off, 0, (int)scalar, for_a);
}


/* LUAR: `s.f = s.f OP k` for an 8-byte integer struct field. */
static int match_cdata_field_update_const (Proto *p, LuaR_JIT *J,
                                           ptrdiff_t start,
                                           ptrdiff_t backedge, int for_a,
                                           TValue *b) {
  Instruction g, op, set;
  int tmp, sreg, kidx, opc, code = -1;
  int64_t scalar = 0;
  size_t off = 0, fsize = 0;
  if (start + 4 != backedge)
    return 0;
  g = p->code[start];
  op = p->code[start + 1];
  set = p->code[start + 3];
  if (GET_OPCODE(g) != OP_GETFIELD || GET_OPCODE(set) != OP_SETFIELD ||
      (GET_OPCODE(p->code[start + 2]) != OP_MMBINI &&
       GET_OPCODE(p->code[start + 2]) != OP_MMBINK &&
       GET_OPCODE(p->code[start + 2]) != OP_MMBIN))
    return 0;
  tmp = GETARG_A(g);
  sreg = GETARG_B(g);
  kidx = GETARG_C(g);
  if (kidx < 0 || kidx >= p->sizek || !ttisstring(&p->k[kidx]) ||
      GETARG_A(set) != sreg || GETARG_B(set) != kidx ||
      GETARG_C(set) != tmp || GETARG_k(set) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp ||
      is_control_reg(tmp, for_a) || is_control_reg(sreg, for_a) ||
      luar_ffi_trace_kind(&b[sreg]) != 3 ||
      !luar_ffi_trace_field_int(&b[sreg], getstr(tsvalue(&p->k[kidx])),
                                &off, &fsize) ||
      fsize == 0)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADDI) {
    code = 0; scalar = GETARG_sC(op);
  }
  else if (opc == OP_SHLI) {
    code = 9; scalar = GETARG_sC(op);
  }
  else if (opc == OP_SHRI) {
    int64_t n = GETARG_sC(op);
    code = (n < 0) ? 9 : 10;
    scalar = (n < 0) ? -n : n;
  }
  else if (opc == OP_ADDK || opc == OP_SUBK || opc == OP_MULK ||
           opc == OP_BANDK || opc == OP_BORK || opc == OP_BXORK ||
           opc == OP_IDIVK || opc == OP_MODK) {
    int idx = GETARG_C(op);
    if (idx < 0 || idx >= p->sizek || !ttisinteger(&p->k[idx]))
      return 0;
    scalar = ivalue(&p->k[idx]);
    code = (opc == OP_ADDK) ? 0 : (opc == OP_SUBK) ? 1 :
           (opc == OP_MULK) ? 2 : (opc == OP_IDIVK) ? 3 :
           (opc == OP_MODK) ? 4 : (opc == OP_BANDK) ? 6 :
           (opc == OP_BORK) ? 7 : 8;
  }
  else
    return 0;
  if (scalar < INT32_MIN || scalar > INT32_MAX || off > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 76, sreg, code, tmp,
                            (int)off, 0, (int)scalar, for_a);
}


/* LUAR: `U[i] = U[i] OP V` with an upvalue array and cdata scalar. */
static int match_cdata_upval_update_scalar (Proto *p, LuaR_JIT *J,
                                            ptrdiff_t start,
                                            ptrdiff_t backedge, int for_a,
                                            TValue *b) {
  ptrdiff_t n = backedge - start + 1, pos = start + 3;
  Instruction up1, up2, g, op, mm, s;
  (void)b;
  int dreg, sreg, tmp, scalar_reg, scalar_enc = 0, opc, code;
  if (n != 7 && n != 8)
    return 0;
  up1 = p->code[start];
  up2 = p->code[start + 1];
  g = p->code[start + 2];
  if (GET_OPCODE(up1) != OP_GETUPVAL || GET_OPCODE(up2) != OP_GETUPVAL ||
      GETARG_B(up1) != GETARG_B(up2) ||
      GET_OPCODE(g) != OP_GETTABLE)
    return 0;
  dreg = GETARG_A(up1);
  sreg = GETARG_A(up2);
  tmp = GETARG_A(g);
  if (GETARG_B(g) != sreg || GETARG_C(g) != for_a + 2 ||
      is_control_reg(dreg, for_a) || is_control_reg(sreg, for_a) ||
      is_control_reg(tmp, for_a))
    return 0;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    scalar_reg = GETARG_A(up);
    scalar_enc = -1 - GETARG_B(up);
    if (is_control_reg(scalar_reg, for_a))
      return 0;
  }
  if (pos + 3 != backedge)
    return 0;
  op = p->code[pos++];
  mm = p->code[pos++];
  s = p->code[pos++];
  if (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
      GET_OPCODE(mm) != OP_MMBINK)
    return 0;
  if (GET_OPCODE(s) != OP_SETTABLE || GETARG_A(s) != dreg ||
      GETARG_B(s) != for_a + 2 || GETARG_C(s) != tmp || GETARG_k(s) ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp)
    return 0;
  if (scalar_enc == 0) {
    scalar_reg = GETARG_C(op);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == dreg ||
        scalar_reg == sreg || scalar_reg == tmp)
      return 0;
    scalar_enc = scalar_reg;
  }
  else if (GETARG_C(op) != scalar_reg)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADD) code = 0;
  else if (opc == OP_SUB) code = 1;
  else if (opc == OP_MUL) code = 2;
  else if (opc == OP_BAND) code = 6;
  else if (opc == OP_BOR) code = 7;
  else if (opc == OP_BXOR) code = 8;
  else if (opc == OP_SHL) code = 9;
  else if (opc == OP_SHR) code = 10;
  else if (opc == OP_IDIV) code = 3;
  else if (opc == OP_MOD) code = 4;
  else return 0;
  return install_cdata_tint(p, J, backedge, 75, dreg, scalar_enc, tmp,
                            code, 0, 0, for_a);
}


/* LUAR: `U[i] = U[i] OP k` with an upvalue array and constant scalar. */
static int match_cdata_upval_update_const (Proto *p, LuaR_JIT *J,
                                           ptrdiff_t start,
                                           ptrdiff_t backedge, int for_a,
                                           TValue *b) {
  Instruction up1, up2, g, op, s;
  int dreg, sreg, tmp, opc, code = -1;
  int64_t scalar = 0;
  if (start + 6 != backedge)
    return 0;
  up1 = p->code[start];
  up2 = p->code[start + 1];
  g = p->code[start + 2];
  op = p->code[start + 3];
  s = p->code[start + 5];
  dreg = GETARG_A(up1);
  sreg = GETARG_A(up2);
  tmp = GETARG_A(g);
  if (GET_OPCODE(up1) != OP_GETUPVAL || GET_OPCODE(up2) != OP_GETUPVAL ||
      GETARG_B(up1) != GETARG_B(up2) ||
      GET_OPCODE(g) != OP_GETTABLE ||
      (GET_OPCODE(p->code[start + 4]) != OP_MMBINI &&
       GET_OPCODE(p->code[start + 4]) != OP_MMBINK &&
       GET_OPCODE(p->code[start + 4]) != OP_MMBIN) ||
      GET_OPCODE(s) != OP_SETTABLE ||
      GETARG_B(g) != sreg || GETARG_C(g) != for_a + 2 ||
      GETARG_A(op) != tmp || GETARG_B(op) != tmp ||
      GETARG_A(s) != dreg || GETARG_B(s) != for_a + 2 ||
      GETARG_C(s) != tmp || GETARG_k(s) ||
      is_control_reg(dreg, for_a) || is_control_reg(sreg, for_a) ||
      is_control_reg(tmp, for_a) ||
      luar_ffi_trace_array_kind64(&b[dreg]) == 0)
    return 0;
  opc = GET_OPCODE(op);
  if (opc == OP_ADDI) {
    code = 0; scalar = GETARG_sC(op);
  }
  else if (opc == OP_SHLI) {
    code = 9; scalar = GETARG_sC(op);
  }
  else if (opc == OP_SHRI) {
    int64_t n = GETARG_sC(op);
    code = (n < 0) ? 9 : 10;
    scalar = (n < 0) ? -n : n;
  }
  else if (opc == OP_ADDK || opc == OP_SUBK || opc == OP_MULK ||
           opc == OP_BANDK || opc == OP_BORK || opc == OP_BXORK ||
           opc == OP_IDIVK || opc == OP_MODK) {
    int idx = GETARG_C(op);
    if (idx < 0 || idx >= p->sizek || !ttisinteger(&p->k[idx]))
      return 0;
    scalar = ivalue(&p->k[idx]);
    code = (opc == OP_ADDK) ? 0 : (opc == OP_SUBK) ? 1 :
           (opc == OP_MULK) ? 2 : (opc == OP_IDIVK) ? 3 :
           (opc == OP_MODK) ? 4 : (opc == OP_BANDK) ? 6 :
           (opc == OP_BORK) ? 7 : 8;
  }
  else
    return 0;
  if (scalar < INT32_MIN || scalar > INT32_MAX)
    return 0;
  return install_cdata_tint(p, J, backedge, 73, dreg, 0, tmp,
                            code, 0, (int)scalar, for_a);
}


/* LUAR: cdata stores whose destination array is a closure upvalue.  The
** compiler emits GETUPVAL dest followed by the normal body and SETTABLE
** dest; the first iteration refreshes the destination slot. */
static int match_cdata_upval_body (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t start, ptrdiff_t backedge,
                                   int for_a, TValue *b) {
  ptrdiff_t body = start + 1;
  Instruction up;
  int dreg, j;
  if (start >= backedge || GET_OPCODE(p->code[start]) != OP_GETUPVAL)
    return 0;
  up = p->code[start];
  dreg = GETARG_A(up);
  if (is_control_reg(dreg, for_a))
    return 0;
  for (j = 0; j < (int)(backedge - body); j++) {
    Instruction s = p->code[start + 1 + j];
    if (GET_OPCODE(s) == OP_SETTABLE && GETARG_A(s) == dreg &&
        GETARG_B(s) == for_a + 2)
      break;
  }
  if (j >= (int)(backedge - body))
    return 0;
  if (match_cdata_unary_store(p, J, body, backedge, for_a, b))
    return 1;
  if (match_cdata_pair_store(p, J, body, backedge, for_a, b))
    return 1;
  if (match_cdata_store(p, J, body, backedge, for_a, b))
    return 1;
  if (match_cdata_scalar_store(p, J, body, backedge, for_a, b))
    return 1;
  return 0;
}


/* Match `s = s + obj.field[i]` where `obj` is a table in a register and
** `field` is a constant string key naming a raw table field.  The helper
** re-reads the field on every entry, so replacing it or changing the array
** shape falls back to the interpreter.  The GETFIELD/GETTABLE destination
** register keeps the last element after the run. */
static int match_field_table_sum (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                                  ptrdiff_t backedge, int for_a, TValue *b) {
  ptrdiff_t n = backedge - start + 1;
  Instruction i0, i1, i2, i3;
  int treg, obj, keyidx, tmp, t2, keyreg, acc;
  TValue *objv, field;
  if (n != 5)
    return 0;
  i0 = p->code[start];
  i1 = p->code[start + 1];
  i2 = p->code[start + 2];
  i3 = p->code[start + 3];
  if (GET_OPCODE(i0) != OP_GETFIELD ||
      GET_OPCODE(i1) != OP_GETTABLE ||
      GET_OPCODE(i2) != OP_ADD ||
      GET_OPCODE(i3) != OP_MMBIN)
    return 0;
  treg = GETARG_A(i0);
  obj = GETARG_B(i0);
  keyidx = GETARG_C(i0);
  tmp = GETARG_A(i1);
  t2 = GETARG_B(i1);
  keyreg = GETARG_C(i1);
  acc = GETARG_A(i2);
  if (keyidx < 0 || keyidx >= p->sizek ||
      !ttisstring(&p->k[keyidx]))
    return 0;
  if (tmp != treg || t2 != treg || keyreg != for_a + 2)
    return 0;
  if (GETARG_A(i2) != GETARG_B(i2) || GETARG_C(i2) != tmp)
    return 0;
  if (tmp == acc || is_control_reg(tmp, for_a) ||
      is_control_reg(acc, for_a) || is_control_reg(obj, for_a))
    return 0;
  objv = &b[obj];
  if (!ttistable(objv))
    return 0;
  memset(&field, 0, sizeof(field));
  if (luaH_getstr(hvalue(objv), tsvalue(&p->k[keyidx]), &field) ==
      LUA_TNIL)
    return 0;
  if (!ttistable(&field))
    return 0;
  return install_cdata_tint(p, J, backedge, 26, obj, keyidx, tmp,
                            acc, tmp, 0, for_a);
}


/* A raw-array read used by the pair intrinsics.  The table can be a
** register local/parameter or a closure upvalue; the value lands in a
** separate register (the compiler may reuse that register for the table
** load of an upvalue). */
typedef struct {
  int table;   /* register holding the table, or -1 for an upvalue */
  int upval;   /* upvalue index, or -1 for a register */
  int val;     /* register receiving the element */
  int treg;    /* GETUPVAL destination (== table for a register read) */
} LuaR_PairRead;


static int match_pair_read (Proto *p, ptrdiff_t *pos, ptrdiff_t backedge,
                            int for_a, LuaR_PairRead *r) {
  Instruction ins;
  if (*pos >= backedge)
    return 0;
  ins = p->code[*pos];
  if (GET_OPCODE(ins) == OP_GETUPVAL) {
    Instruction gt;
    int reg = GETARG_A(ins);
    if (*pos + 1 >= backedge)
      return 0;
    gt = p->code[*pos + 1];
    if (GET_OPCODE(gt) != OP_GETTABLE ||
        GETARG_B(gt) != reg || GETARG_C(gt) != for_a + 2)
      return 0;
    r->table = -1;
    r->upval = GETARG_B(ins);
    r->treg = reg;
    r->val = GETARG_A(gt);
    *pos += 2;
    return 1;
  }
  if (GET_OPCODE(ins) == OP_GETTABLE) {
    if (GETARG_C(ins) != for_a + 2)
      return 0;
    r->table = GETARG_B(ins);
    r->upval = -1;
    r->treg = GETARG_B(ins);
    r->val = GETARG_A(ins);
    *pos += 1;
    return 1;
  }
  return 0;
}


static int encode_pair_source (const LuaR_PairRead *r) {
  return (r->upval >= 0) ? (-1 - r->upval) : r->table;
}


/* Resolve a pair source to its live value: a register slot or a closure
** upvalue.  Used both to choose a table/cdata intrinsic and to verify that
** the source still has the shape the intrinsic expects. */
static TValue *pair_source_value (TValue *base, int reg, int upval) {
  if (upval >= 0) {
    TValue *fv = &base[-1];
    LClosure *cl;
    if (!ttisLclosure(fv))
      return NULL;
    cl = clLvalue(fv);
    if (upval >= cl->p->sizeupvalues)
      return NULL;
    return cl->upvals[upval]->v.p;
  }
  if (reg < 0 || reg > 255)
    return NULL;
  return &base[reg];
}


/* A cdata array source is one of the bounded 1/2/4-byte integer or
** 4/8-byte float arrays accepted by the FFI trace helpers.  A table source
** returns 0. */
static int pair_cdata_source (TValue *base, const LuaR_PairRead *r) {
  TValue *tv = pair_source_value(base, r->table, r->upval);
  return (tv != NULL) ? luar_ffi_trace_array_kind(tv) : 0;
}


/* LUAR: `s = s + a[i] + b[i] + c[i]` over three raw arrays.  Each source
** may be a register table or a closure upvalue; the compiler emits a
** GETUPVAL before the GETTABLE for an upvalue read. */
static int install_three_sum (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                              int for_a, const LuaR_PairRead *r1,
                              const LuaR_PairRead *r2,
                              const LuaR_PairRead *r3, int acc, int res) {
  int slot = -1, k;
  if (res == acc)
    return 0;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  p->luar_tint[slot] = 11;
  p->luar_tint_suspend[slot] = 0;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = encode_pair_source(r1);
  p->luar_tint_keys[slot] = encode_pair_source(r2);
  p->luar_tint_upval[slot] = encode_pair_source(r3);
  p->luar_tint_acc[slot] = acc;
  p->luar_tint_res[slot] = res;
  /* The intermediate (sumreg) is r1->val and the second element is
  ** r2->val; both are bytecode-visible after the loop. */
  p->luar_tint_arg4[slot] = (r1->val & 0xff) | ((r2->val & 0xff) << 8);
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


/* LUAR: `s = s + a[i] * k` / `s = s + a[i] / k` over a bounded cdata
** array.  Kinds 30 and 31 distinguish multiply from divide; the source,
** scalar, element and product slots use the same encoding as the raw-table
** scaled reduction. */
static int install_cdata_scaled (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                                 int for_a, const LuaR_PairRead *r,
                                 int scalar_enc, int elem_reg,
                                 int prod_reg, int acc, int div, int sub) {
  int kind = (sub ? 34 : 30) + (div ? 1 : 0);
  return install_cdata_tint(p, J, backedge, kind,
                            encode_pair_source(r), scalar_enc, elem_reg,
                            acc, prod_reg, 0, for_a);
}


/* LUAR: `s = s + a[i] * k` or `s = s + a[i] / k` where `k` is a numeric
** constant, a register scalar or a closure upvalue scalar.  The scalar
** encoding is positive for a register, -1-index for an upvalue and
** 0x40000000|k for a Proto constant.  Kind 15 is multiply, kind 28 is
** divide; only the left operand of `/` is ever the element because
** division does not commute. */
#define LUAR_TINT_SCALAR_CONST 0x40000000
#define LUAR_TINT_SCALAR_IMM   0x20000000

static int install_scaled_sum (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                               int for_a, const LuaR_PairRead *r,
                               int scalar_enc, int elem_reg, int prod_reg,
                               int acc, int div, int sub) {
  int slot = -1, k;
  if (prod_reg == acc)
    return 0;
  for (k = 0; k < 2; k++) {
    if (p->luar_code[k] == NULL && p->luar_tint[k] == 0) {
      slot = k;
      break;
    }
  }
  if (slot < 0)
    slot = 1;
  p->luar_code[slot] = NULL;
  /* 15/28: multiply/divide, add; 32/33: multiply/divide, subtract. */
  p->luar_tint[slot] = sub ? (div ? 33 : 32) : (div ? 28 : 15);
  p->luar_tint_suspend[slot] = 0;
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = encode_pair_source(r);
  p->luar_tint_keys[slot] = scalar_enc;
  p->luar_tint_upval[slot] = elem_reg;
  p->luar_tint_arg4[slot] = prod_reg;
  p->luar_tint_acc[slot] = acc;
  p->luar_tint_res[slot] = prod_reg;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


/* A register table source must not have been overwritten before its
** GETTABLE.  An upvalue source is reloaded by GETUPVAL into its own
** register, so only the read destinations matter there. */
static int three_source_ok (const LuaR_PairRead *src,
                            const LuaR_PairRead *prev, int nprev,
                            int sumreg, int acc, int first) {
  int j;
  if (src->treg == acc)
    return 0;
  if (!first && src->treg == sumreg)
    return 0;
  /* A register table must survive until its GETTABLE.  An upvalue read
  ** reloads its table into treg, so only the still-live accumulator and
  ** intermediate matter there; earlier element temporaries are dead. */
  if (src->upval >= 0)
    return 1;
  for (j = 0; j < nprev; j++)
    if (src->treg == prev[j].val)
      return 0;
  return 1;
}


static int match_three_table_sum (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                                  ptrdiff_t backedge, int for_a, TValue *b) {
  LuaR_PairRead r[3];
  Instruction add1, add2, add3;
  ptrdiff_t i = start;
  int sumreg, acc;
  if (!match_pair_read(p, &i, backedge, for_a, &r[0]))
    return 0;
  if (i >= backedge)
    return 0;
  add1 = p->code[i++];
  if (GET_OPCODE(add1) != OP_ADD || GETARG_A(add1) != r[0].val ||
      GETARG_C(add1) != r[0].val)
    return 0;
  sumreg = GETARG_A(add1);
  acc = GETARG_B(add1);
  if (sumreg == acc || is_control_reg(sumreg, for_a) ||
      is_control_reg(acc, for_a) || is_control_reg(r[0].val, for_a))
    return 0;
  if (r[0].treg == acc)
    return 0;
  if (i >= backedge || GET_OPCODE(p->code[i]) != OP_MMBIN)
    return 0;
  i++;
  if (!match_pair_read(p, &i, backedge, for_a, &r[1]))
    return 0;
  if (r[1].val == sumreg || r[1].val == acc ||
      is_control_reg(r[1].val, for_a))
    return 0;
  if (!three_source_ok(&r[1], r, 1, sumreg, acc, 0))
    return 0;
  if (i >= backedge)
    return 0;
  add2 = p->code[i++];
  if (GET_OPCODE(add2) != OP_ADD || GETARG_A(add2) != sumreg ||
      GETARG_B(add2) != sumreg || GETARG_C(add2) != r[1].val)
    return 0;
  if (i >= backedge || GET_OPCODE(p->code[i]) != OP_MMBIN)
    return 0;
  i++;
  if (!match_pair_read(p, &i, backedge, for_a, &r[2]))
    return 0;
  if (r[2].val == sumreg || r[2].val == acc ||
      is_control_reg(r[2].val, for_a))
    return 0;
  if (!three_source_ok(&r[2], r, 2, sumreg, acc, 0))
    return 0;
  if (i >= backedge)
    return 0;
  add3 = p->code[i++];
  if (GET_OPCODE(add3) != OP_ADD || GETARG_A(add3) != acc)
    return 0;
  if (!((GETARG_B(add3) == sumreg && GETARG_C(add3) == r[2].val) ||
        (GETARG_B(add3) == r[2].val && GETARG_C(add3) == sumreg)))
    return 0;
  if (i >= backedge || GET_OPCODE(p->code[i]) != OP_MMBIN)
    return 0;
  i++;
  if (i != backedge)
    return 0;
  if (acc == r[0].val || acc == r[1].val || acc == r[2].val)
    return 0;
  /* A register read whose destination is its own table register is the
  ** 't = t[i]' shape.  It is only meaningful for the first read; later
  ** reads must keep their source table alive. */
  {
    int j;
    for (j = 1; j < 3; j++)
      if (r[j].upval < 0 && r[j].table == r[j].val)
        return 0;
  }
  if (!ttisinteger(&b[acc]) && !ttisfloat(&b[acc]))
    return 0;
  /* Register sources must hold a table at entry; upvalue sources are
  ** re-resolved by the intrinsic at every entry. */
  {
    int j;
    for (j = 0; j < 3; j++) {
      if (r[j].upval < 0 && !ttistable(&b[r[j].table]))
        return 0;
    }
  }
  return install_three_sum(p, J, backedge, for_a, &r[0], &r[1], &r[2],
                           acc, r[2].val);
}


static int install_pair_intrinsic (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                                   int for_a, const LuaR_PairRead *ra,
                                   const LuaR_PairRead *rb, int acc, int res,
                                   int kind) {
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
  p->luar_tint[slot] = kind;
  p->luar_tint_suspend[slot] = 0;
  p->luar_tint_pc[slot] = (int)backedge;
  /* Pack each source into one int: a register is the value itself, an
  ** upvalue is -1-index.  The two remaining slots keep the element
  ** registers that the bytecode can observe after the loop. */
  p->luar_tint_table[slot] =
      (ra->upval >= 0) ? (-1 - ra->upval) : ra->table;
  p->luar_tint_keys[slot] =
      (rb->upval >= 0) ? (-1 - rb->upval) : rb->table;
  p->luar_tint_upval[slot] = ra->val;
  p->luar_tint_arg4[slot] = rb->val;
  p->luar_tint_acc[slot] = acc;
  p->luar_tint_res[slot] = res;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


/* LUAR: `s = s + a[i] + b[i]` / `s = s + a[i] - b[i]` over two raw arrays.
** The compiler emits the left-associative shape as one ADD into a temporary
** followed by the final ADD/SUB into the accumulator.  Both arrays may come
** from registers or closure upvalues, which the generic register-table
** emitter does not cover. */
static int match_pair_sum (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                           ptrdiff_t backedge, int for_a, TValue *base) {
  LuaR_PairRead ra, rb;
  Instruction op1, mm1, op2, mm2;
  ptrdiff_t i = start;
  int t1, acc, b1, c1, sub;
  if (!match_pair_read(p, &i, backedge, for_a, &ra))
    return 0;
  if (i >= backedge)
    return 0;
  op1 = p->code[i];
  if (GET_OPCODE(op1) != OP_ADD)
    return 0;
  t1 = GETARG_A(op1);
  b1 = GETARG_B(op1);
  c1 = GETARG_C(op1);
  if (b1 == ra.val)
    acc = c1;
  else if (c1 == ra.val)
    acc = b1;
  else
    return 0;
  if (is_control_reg(t1, for_a) || is_control_reg(acc, for_a) ||
      t1 == for_a + 2 || acc == for_a + 2 || ra.val == acc ||
      ra.treg == acc)
    return 0;
  i++;
  if (i >= backedge)
    return 0;
  mm1 = p->code[i];
  if (GET_OPCODE(mm1) != OP_MMBIN)
    return 0;
  i++;
  if (!match_pair_read(p, &i, backedge, for_a, &rb))
    return 0;
  /* The two element reads must not overwrite each other or the cached
  ** accumulator before the final arithmetic.  An upvalue read also writes
  ** its GETUPVAL table temporary, so check that register too. */
  if (rb.val == acc || rb.val == ra.val || rb.val == t1 ||
      rb.treg == acc || rb.treg == ra.val || rb.treg == t1)
    return 0;
  if (i >= backedge)
    return 0;
  op2 = p->code[i];
  if (GETARG_A(op2) != acc)
    return 0;
  if (GET_OPCODE(op2) == OP_SUB) {
    if (GETARG_B(op2) != t1 || GETARG_C(op2) != rb.val)
      return 0;
    sub = 1;
  }
  else if (GET_OPCODE(op2) == OP_ADD) {
    if (!((GETARG_B(op2) == t1 && GETARG_C(op2) == rb.val) ||
          (GETARG_B(op2) == rb.val && GETARG_C(op2) == t1)))
      return 0;
    sub = 0;
  }
  else
    return 0;
  i++;
  if (i >= backedge)
    return 0;
  mm2 = p->code[i];
  if (GET_OPCODE(mm2) != OP_MMBIN)
    return 0;
  i++;
  if (i != backedge)
    return 0;
  if (is_control_reg(ra.val, for_a) || is_control_reg(rb.val, for_a))
    return 0;
  {
    /* The bytecode shape is the same for Lua tables and bounded cdata
    ** arrays; choose the helper from the live source values.  A mixed
    ** table/cdata pair has no common reduction, so leave it interpreted. */
    int acd = pair_cdata_source(base, &ra);
    int bcd = pair_cdata_source(base, &rb);
    if ((acd != 0) != (bcd != 0))
      return 0;
    if (acd != 0)
      return install_pair_intrinsic(p, J, backedge, for_a, &ra, &rb, acc,
                                    t1, sub ? 18 : 17);
    return install_pair_intrinsic(p, J, backedge, for_a, &ra, &rb, acc, t1,
                                  sub ? 13 : 12);
  }
}


/* LUAR: `s = s + a[i] * k` with a numeric constant or a scalar held in a
** register/upvalue.  This is the weighted-sum shape; the generic table
** decoder only handles the two-array product. */
static int match_scaled_sum (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                             ptrdiff_t backedge, int for_a, TValue *b) {
  LuaR_PairRead r;
  Instruction mul, add;
  ptrdiff_t i = start;
  int mul_a, mul_b, mul_c, scalar_enc = -1, scalar_reg = -1;
  int scalar_pre = -1;
  int acc, prod_reg;
  int div = 0, sub = 0;
  if (!match_pair_read(p, &i, backedge, for_a, &r))
    return 0;
  if (i >= backedge)
    return 0;
  /* An upvalue scalar is loaded just before the multiply. */
  if (GET_OPCODE(p->code[i]) == OP_GETUPVAL) {
    Instruction up = p->code[i++];
    int sreg = GETARG_A(up);
    if (i >= backedge || sreg == r.val || is_control_reg(sreg, for_a))
      return 0;
    scalar_reg = sreg;
    scalar_pre = sreg;
    scalar_enc = -1 - GETARG_B(up);
  }
  if (i >= backedge)
    return 0;
  mul = p->code[i++];
  if (GET_OPCODE(mul) == OP_DIVK)
    div = 1;
  else if (GET_OPCODE(mul) == OP_DIV)
    div = 1;
  if (GET_OPCODE(mul) == OP_MULK || GET_OPCODE(mul) == OP_DIVK) {
    int kidx = GETARG_C(mul);
    TValue *k;
    if (GETARG_B(mul) != r.val || kidx >= p->sizek ||
        scalar_enc != -1)
      return 0;
    k = &p->k[kidx];
    if (!ttisinteger(k) && !ttisfloat(k))
      return 0;
    if (kidx >= LUAR_TINT_SCALAR_CONST)
      return 0;
    scalar_enc = LUAR_TINT_SCALAR_CONST | kidx;
    mul_a = GETARG_A(mul);
  }
  else if (GET_OPCODE(mul) == OP_MUL || GET_OPCODE(mul) == OP_DIV) {
    mul_a = GETARG_A(mul);
    mul_b = GETARG_B(mul);
    mul_c = GETARG_C(mul);
    if (mul_b == r.val)
      scalar_reg = mul_c;
    else if (mul_c == r.val && GET_OPCODE(mul) == OP_MUL)
      scalar_reg = mul_b;   /* only multiplication commutes */
    else
      return 0;
    if (scalar_pre >= 0) {
      if (scalar_reg != scalar_pre)
        return 0;
    }
    else {
      if (scalar_reg < 0 || scalar_reg > 255 ||
          is_control_reg(scalar_reg, for_a))
        return 0;
      scalar_enc = scalar_reg;
    }
  }
  else
    return 0;
  if (i >= backedge ||
      (GET_OPCODE(p->code[i]) != OP_MMBIN &&
       GET_OPCODE(p->code[i]) != OP_MMBINK))
    return 0;
  i++;
  if (i >= backedge)
    return 0;
  add = p->code[i++];
  acc = GETARG_A(add);
  if (GET_OPCODE(add) == OP_SUB) {
    /* `s - product`; `product - s` is a different reduction. */
    if (GETARG_B(add) != acc || GETARG_C(add) != mul_a)
      return 0;
    sub = 1;
  }
  else if (GET_OPCODE(add) == OP_ADD) {
    if (!((GETARG_B(add) == acc && GETARG_C(add) == mul_a) ||
          (GETARG_B(add) == mul_a && GETARG_C(add) == acc)))
      return 0;
  }
  else
    return 0;
  if (i >= backedge || GET_OPCODE(p->code[i]) != OP_MMBIN)
    return 0;
  i++;
  if (i != backedge)
    return 0;
  prod_reg = mul_a;
  if (acc == r.val || acc == prod_reg)
    return 0;
  if (is_control_reg(acc, for_a) || is_control_reg(prod_reg, for_a) ||
      is_control_reg(r.val, for_a))
    return 0;
  if (r.treg == acc || r.treg == scalar_reg)
    return 0;
  /* The element/product temp must not overwrite the source register: the
  ** interpreter would then index the quotient or element on the next
  ** iteration, while the intrinsic would keep using the original table or
  ** array and produce a different result.  The element read itself must
  ** not reuse the source register either. */
  if (r.upval < 0 && (r.treg == r.val || r.treg == prod_reg))
    return 0;
  if (scalar_enc >= 0 && scalar_enc < LUAR_TINT_SCALAR_CONST) {
    /* A register scalar must not be overwritten by the element read or
    ** the multiply; otherwise later iterations see a different value. */
    if (scalar_reg == r.val || scalar_reg == prod_reg || scalar_reg == acc)
      return 0;
    if (!ttisinteger(&b[scalar_reg]) && !ttisfloat(&b[scalar_reg]))
      return 0;
  }
  {
    TValue *src = pair_source_value(b, r.table, r.upval);
    int ckind;
    if (src == NULL)
      return 0;
    ckind = pair_cdata_source(b, &r);
    if (ckind != 0)
      return install_cdata_scaled(p, J, backedge, for_a, &r, scalar_enc,
                                  r.val, prod_reg, acc, div, sub);
    if (!ttistable(src))
      return 0;
  }
  if (!ttisinteger(&b[acc]) && !ttisfloat(&b[acc]))
    return 0;
  return install_scaled_sum(p, J, backedge, for_a, &r, scalar_enc,
                            r.val, prod_reg, acc, div, sub);
}


/* LUAR: `U[i] = nil` where the destination table comes from a closure
** upvalue.  The compiler emits GETUPVAL dest; SETTABLE dest,key,nil. */
static int match_table_upval_nil (Proto *p, LuaR_JIT *J,
                                  ptrdiff_t start, ptrdiff_t backedge,
                                  int for_a, TValue *b) {
  ptrdiff_t pos = start;
  Instruction u, s;
  int key = for_a + 2, dreg;
  if (pos >= backedge || GET_OPCODE(p->code[pos]) != OP_GETUPVAL)
    return 0;
  u = p->code[pos++];
  dreg = GETARG_A(u);
  if (is_control_reg(dreg, for_a) || !ttistable(&b[dreg]))
    return 0;
  if (pos >= backedge)
    return 0;
  s = p->code[pos++];
  if (GET_OPCODE(s) != OP_SETTABLE || GETARG_A(s) != dreg ||
      GETARG_B(s) != key || !TESTARG_k(s) ||
      GETARG_C(s) >= p->sizek || !ttisnil(&p->k[GETARG_C(s)]) ||
      pos != backedge)
    return 0;
  return install_cdata_tint(p, J, backedge, 58, dreg, 0, 0, 0, 0, 0,
                            for_a);
}


/* LUAR: `U[i] = src[i]` where the destination table comes from a closure
** upvalue.  The compiler emits GETUPVAL dest; GETTABLE src; SETTABLE
** dest.  The destination slot is re-read from the closure on every entry. */
static int match_table_upval_copy (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t start, ptrdiff_t backedge,
                                   int for_a, TValue *b) {
  ptrdiff_t pos = start;
  Instruction u, g, s;
  int key = for_a + 2, dreg, tmp, src, src_up_enc = 0, src_up_reg = -1;
  if (pos >= backedge || GET_OPCODE(p->code[pos]) != OP_GETUPVAL)
    return 0;
  u = p->code[pos++];
  dreg = GETARG_A(u);
  if (is_control_reg(dreg, for_a) || !ttistable(&b[dreg]))
    return 0;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    src_up_reg = GETARG_A(up);
    if (is_control_reg(src_up_reg, for_a))
      return 0;
    src_up_enc = -1 - GETARG_B(up);
  }
  if (pos >= backedge || GET_OPCODE(p->code[pos]) != OP_GETTABLE)
    return 0;
  g = p->code[pos++];
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  if (src_up_enc != 0 && src != src_up_reg)
    return 0;
  if (GETARG_C(g) != key || tmp == key || tmp == dreg ||
      (src_up_enc == 0 && tmp == src) ||
      is_control_reg(tmp, for_a) || is_control_reg(src, for_a))
    return 0;
  {
    int src_enc = (src_up_enc != 0) ? src_up_enc : src;
    TValue *sv = pair_source_value(b, (src_enc >= 0) ? src_enc : -1,
                                   (src_enc >= 0) ? -1 : (-1 - src_enc));
    if (sv == NULL || !ttistable(sv))
      return 0;
    if (pos >= backedge)
      return 0;
    s = p->code[pos++];
    if (GET_OPCODE(s) != OP_SETTABLE || GETARG_k(s) ||
        GETARG_A(s) != dreg || GETARG_B(s) != key ||
        GETARG_C(s) != tmp || pos != backedge)
      return 0;
    return install_cdata_tint(p, J, backedge, 57, dreg, src_enc, tmp,
                              0, 0, 0, for_a);
  }
}


static int match_table_nil_store (Proto *p, LuaR_JIT *J,
                                  ptrdiff_t start, ptrdiff_t backedge,
                                  int for_a, TValue *b);
static int match_table_unary_store (Proto *p, LuaR_JIT *J,
                                    ptrdiff_t start, ptrdiff_t backedge,
                                    int for_a, TValue *b);
static int match_table_pair_store (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t start, ptrdiff_t backedge,
                                   int for_a, TValue *b);
static int match_table_scaled_store (Proto *p, LuaR_JIT *J,
                                     ptrdiff_t start, ptrdiff_t backedge,
                                     int for_a, TValue *b);

/* LUAR: table stores whose destination is a closure-upvalue table.  The
** compiler emits GETUPVAL dest followed by the normal body and SETTABLE
** dest.  The prefix is outside the intrinsic range, so the first body
** iteration refreshes the destination slot; the helper re-checks the slot
** on every entry. */
static int match_table_upval_body (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t start, ptrdiff_t backedge,
                                   int for_a, TValue *b) {
  ptrdiff_t body = start + 1;
  Instruction up;
  int dreg, j;
  if (start >= backedge || GET_OPCODE(p->code[start]) != OP_GETUPVAL)
    return 0;
  up = p->code[start];
  dreg = GETARG_A(up);
  if (is_control_reg(dreg, for_a) || !ttistable(&b[dreg]))
    return 0;
  for (j = 0; j < (int)(backedge - body); j++) {
    Instruction s = p->code[start + 1 + j];
    if (GET_OPCODE(s) == OP_SETTABLE && GETARG_A(s) == dreg &&
        GETARG_B(s) == for_a + 2)
      break;
  }
  if (j >= (int)(backedge - body))
    return 0;
  if (match_table_nil_store(p, J, body, backedge, for_a, b))
    return 1;
  if (match_table_unary_store(p, J, body, backedge, for_a, b))
    return 1;
  if (match_table_pair_store(p, J, body, backedge, for_a, b))
    return 1;
  if (match_table_scaled_store(p, J, body, backedge, for_a, b))
    return 1;
  return 0;
}


/* LUAR: fill a table range with a non-numeric constant, register or
** upvalue value (`t[i] = "x"`, `t[i] = obj`, `t[i] = U`).  Numeric fills
** stay with the specialised affine helper. */
static int match_table_any_fill (Proto *p, LuaR_JIT *J,
                                 ptrdiff_t start, ptrdiff_t backedge,
                                 int for_a, TValue *b) {
  ptrdiff_t pos = start;
  int key = for_a + 2, dst = -1, val_enc = -1, val_reg = -1, arg4 = 0;
  int have_val = 0, dest_up = 0;
  Instruction set;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    dst = GETARG_A(up);
    if (is_control_reg(dst, for_a) || !ttistable(&b[dst]))
      return 0;
    dest_up = 1;
  }
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    val_reg = GETARG_A(up);
    if (is_control_reg(val_reg, for_a) || val_reg == dst)
      return 0;
    val_enc = -3 - GETARG_B(up);
    have_val = 1;
  }
  else if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_LOADK) {
    Instruction ld = p->code[pos++];
    val_reg = GETARG_A(ld);
    if (is_control_reg(val_reg, for_a) || val_reg == dst ||
        GETARG_Bx(ld) >= p->sizek)
      return 0;
    val_enc = -1;
    arg4 = GETARG_Bx(ld);
    have_val = 1;
  }
  if (pos >= backedge)
    return 0;
  set = p->code[pos++];
  if (GET_OPCODE(set) != OP_SETTABLE || GETARG_B(set) != key ||
      pos != backedge)
    return 0;
  if (dst < 0) {
    dst = GETARG_A(set);
    if (is_control_reg(dst, for_a) || !ttistable(&b[dst]))
      return 0;
  }
  else if (GETARG_A(set) != dst)
    return 0;
  if (have_val) {
    if (GETARG_k(set) || GETARG_C(set) != val_reg)
      return 0;
  }
  else if (GETARG_k(set)) {
    if (GETARG_C(set) >= p->sizek)
      return 0;
    val_enc = -1;
    arg4 = GETARG_C(set);
  }
  else {
    val_enc = GETARG_C(set);
    val_reg = val_enc;
    if (is_control_reg(val_enc, for_a) || val_enc == dst)
      return 0;
  }
  {
    TValue *vv = (val_enc == -1) ? &p->k[arg4]
                 : (val_enc >= 0) ? &b[val_enc]
                 : pair_source_value(b, -1, -3 - val_enc);
    if (vv == NULL || (ttisnumber(vv) && !dest_up))
      return 0;   /* numeric fills use the affine path */
    if (val_reg == key)
      return 0;   /* t[i] = i varies with the loop index */
  }
  return install_cdata_tint(p, J, backedge, 61, dst, val_enc, val_reg,
                            0, 0, arg4, for_a);
}


/* LUAR: affine fill with a register/upvalue scalar:
** `t[i]/U[i] = i + k`, `i - k` or `i * k`. */
static int match_table_affine_scalar_fill (Proto *p, LuaR_JIT *J,
                                           ptrdiff_t start,
                                           ptrdiff_t backedge, int for_a,
                                           TValue *b) {
  ptrdiff_t pos = start;
  int key = for_a + 2, dst = -1, tmp, mode, scalar_enc = 0, scalar_reg = -1;
  int dest_up = 0;
  Instruction op, mm, set;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos];
    int d = GETARG_A(up), j;
    for (j = (int)(pos + 1); j < (int)backedge; j++)
      if (GET_OPCODE(p->code[j]) == OP_SETTABLE &&
          GETARG_A(p->code[j]) == d && GETARG_B(p->code[j]) == key) {
        dst = d;
        dest_up = 1;
        pos++;
        break;
      }
  }
  if (dest_up && (is_control_reg(dst, for_a) || !ttistable(&b[dst])))
    return 0;
  if (pos < backedge && GET_OPCODE(p->code[pos]) == OP_GETUPVAL) {
    Instruction up = p->code[pos++];
    scalar_reg = GETARG_A(up);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == dst)
      return 0;
    scalar_enc = -1 - GETARG_B(up);
  }
  if (pos >= backedge)
    return 0;
  op = p->code[pos++];
  if (GET_OPCODE(op) == OP_ADD) mode = LUAR_TINT_FILL_ADD;
  else if (GET_OPCODE(op) == OP_SUB) mode = LUAR_TINT_FILL_SUB;
  else if (GET_OPCODE(op) == OP_MUL) mode = LUAR_TINT_FILL_MUL;
  else return 0;
  tmp = GETARG_A(op);
  if (GETARG_B(op) != key || is_control_reg(tmp, for_a))
    return 0;
  if (scalar_enc == 0) {
    scalar_reg = GETARG_C(op);
    if (is_control_reg(scalar_reg, for_a) || scalar_reg == key ||
        scalar_reg == tmp || scalar_reg == dst)
      return 0;
    scalar_enc = scalar_reg;
  }
  else if (GETARG_C(op) != scalar_reg)
    return 0;
  if (scalar_reg >= 0 && !ttisinteger(&b[scalar_reg]) &&
      !ttisfloat(&b[scalar_reg]))
    return 0;
  if (pos >= backedge)
    return 0;
  mm = p->code[pos++];
  if (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
      GET_OPCODE(mm) != OP_MMBINK)
    return 0;
  if (pos >= backedge)
    return 0;
  set = p->code[pos++];
  if (GET_OPCODE(set) != OP_SETTABLE || GETARG_k(set) ||
      GETARG_B(set) != key || GETARG_C(set) != tmp || pos != backedge)
    return 0;
  if (dst < 0) {
    dst = GETARG_A(set);
    if (is_control_reg(dst, for_a) || !ttistable(&b[dst]))
      return 0;
  }
  else if (GETARG_A(set) != dst)
    return 0;
  return install_cdata_tint(p, J, backedge, 63, dst, mode, scalar_enc,
                            0, 0, tmp, for_a);
}


/* LUAR: `t[i] = i + k`, `i - k` or `i * k` over a raw table array. */
static int match_table_affine_fill (Proto *p, LuaR_JIT *J,
                                    ptrdiff_t start, ptrdiff_t backedge,
                                    int for_a, TValue *b) {
  Instruction op, set;
  int key = for_a + 2, dst, mode, scalar;
  if (start + 3 != backedge ||
      (GET_OPCODE(p->code[start + 1]) != OP_MMBINI &&
       GET_OPCODE(p->code[start + 1]) != OP_MMBINK &&
       GET_OPCODE(p->code[start + 1]) != OP_MMBIN))
    return 0;
  op = p->code[start];
  set = p->code[start + 2];
  if (GET_OPCODE(op) == OP_ADDI) {
    mode = LUAR_TINT_FILL_ADD;
    scalar = (int)GETARG_sC(op);
  }
  else if (GET_OPCODE(op) == OP_MULK || GET_OPCODE(op) == OP_SUBK) {
    int kidx = GETARG_C(op);
    if (kidx < 0 || kidx >= p->sizek || !ttisinteger(&p->k[kidx]))
      return 0;
    mode = (GET_OPCODE(op) == OP_SUBK) ? LUAR_TINT_FILL_SUB
                                       : LUAR_TINT_FILL_MUL;
    scalar = (int)ivalue(&p->k[kidx]);
  }
  else
    return 0;
  if (GETARG_B(op) != key ||
      GET_OPCODE(set) != OP_SETTABLE || GETARG_k(set) ||
      GETARG_B(set) != key || GETARG_C(set) != GETARG_A(op))
    return 0;
  dst = GETARG_A(set);
  if (is_control_reg(dst, for_a) || dst == GETARG_A(op) ||
      !ttistable(&b[dst]))
    return 0;
  return install_affine_fill(p, J, backedge, for_a, dst, mode, scalar);
}


/* LUAR: `U[i] = i + k` / `U[i] = i * k` with an upvalue destination. */
static int match_table_upval_affine_fill (Proto *p, LuaR_JIT *J,
                                          ptrdiff_t start, ptrdiff_t backedge,
                                          int for_a, TValue *b) {
  Instruction up, op, set;
  int dreg, key = for_a + 2, mode, scalar;
  if (start + 4 != backedge || GET_OPCODE(p->code[start]) != OP_GETUPVAL)
    return 0;
  up = p->code[start];
  op = p->code[start + 1];
  set = p->code[start + 3];
  dreg = GETARG_A(up);
  if (is_control_reg(dreg, for_a) || !ttistable(&b[dreg]))
    return 0;
  if (GET_OPCODE(p->code[start + 2]) != OP_MMBINI &&
      GET_OPCODE(p->code[start + 2]) != OP_MMBINK &&
      GET_OPCODE(p->code[start + 2]) != OP_MMBIN)
    return 0;
  if (GET_OPCODE(op) == OP_ADDI) {
    mode = LUAR_TINT_FILL_ADD;
    scalar = (int)GETARG_sC(op);
  }
  else if (GET_OPCODE(op) == OP_MULK || GET_OPCODE(op) == OP_SUBK) {
    int kidx = GETARG_C(op);
    if (kidx < 0 || kidx >= p->sizek || !ttisinteger(&p->k[kidx]))
      return 0;
    mode = (GET_OPCODE(op) == OP_SUBK) ? LUAR_TINT_FILL_SUB
                                       : LUAR_TINT_FILL_MUL;
    scalar = (int)ivalue(&p->k[kidx]);
  }
  else
    return 0;
  if (GETARG_B(op) != key || GETARG_A(op) == dreg ||
      GET_OPCODE(set) != OP_SETTABLE || GETARG_A(set) != dreg ||
      GETARG_B(set) != key || GETARG_C(set) != GETARG_A(op) ||
      GETARG_k(set))
    return 0;
  return install_affine_fill(p, J, backedge, for_a, dreg, mode, scalar);
}


/* LUAR: `U[i] = i` with a closure-upvalue destination. */
static int match_table_upval_index_fill (Proto *p, LuaR_JIT *J,
                                         ptrdiff_t start, ptrdiff_t backedge,
                                         int for_a, TValue *b) {
  Instruction up, set;
  int dreg, key = for_a + 2;
  if (start + 2 != backedge || GET_OPCODE(p->code[start]) != OP_GETUPVAL)
    return 0;
  up = p->code[start];
  set = p->code[start + 1];
  dreg = GETARG_A(up);
  if (is_control_reg(dreg, for_a) || !ttistable(&b[dreg]) ||
      GET_OPCODE(set) != OP_SETTABLE || GETARG_A(set) != dreg ||
      GETARG_B(set) != key || GETARG_C(set) != key || GETARG_k(set))
    return 0;
  return install_cdata_tint(p, J, backedge, 62, dreg, 0, 0, 0, 0, 0,
                            for_a);
}


/* LUAR: `dst[i] = nil` over a raw table array.  The run must stay inside
** the existing array part and the table must have no metatable, so the
** interpreter would perform plain nonexistent-key writes. */
static int match_table_nil_store (Proto *p, LuaR_JIT *J,
                                  ptrdiff_t start, ptrdiff_t backedge,
                                  int for_a, TValue *b) {
  Instruction set;
  ptrdiff_t i = start;
  int dst;
  if (i >= backedge)
    return 0;
  set = p->code[i];
  if (GET_OPCODE(set) != OP_SETTABLE ||
      GETARG_B(set) != for_a + 2 || !TESTARG_k(set) ||
      GETARG_C(set) >= p->sizek || !ttisnil(&p->k[GETARG_C(set)]))
    return 0;
  dst = GETARG_A(set);
  if (is_control_reg(dst, for_a) || !ttistable(&b[dst]))
    return 0;
  i++;
  if (i != backedge)
    return 0;
  return install_table_nil_store(p, J, backedge, for_a, dst);
}


/* LUAR: `dst[i] = a[i] OP b[i]` over two raw table arrays.  The source
** tables may be register locals or closure upvalues; the destination may
** be one of them. */
static int match_table_pair_store (Proto *p, LuaR_JIT *J,
                                   ptrdiff_t start, ptrdiff_t backedge,
                                   int for_a, TValue *b) {
  LuaR_PairRead ra, rb;
  Instruction op, mm, set;
  ptrdiff_t i = start;
  int opc, op_a, op_b, op_c, dst, kind;
  if (!match_pair_read(p, &i, backedge, for_a, &ra))
    return 0;
  if (!match_pair_read(p, &i, backedge, for_a, &rb))
    return 0;
  if (ra.val == rb.val || rb.treg == ra.val)
    return 0;
  if (i >= backedge)
    return 0;
  op = p->code[i++];
  opc = GET_OPCODE(op);
  op_a = GETARG_A(op);
  op_b = GETARG_B(op);
  op_c = GETARG_C(op);
  if (opc == OP_ADD) kind = 0;
  else if (opc == OP_SUB) kind = 1;
  else if (opc == OP_MUL) kind = 2;
  else if (opc == OP_DIV) kind = 3;
  else if (opc == OP_IDIV) kind = 5;
  else if (opc == OP_MOD) kind = 6;
  else if (opc == OP_BAND) kind = 7;
  else if (opc == OP_BOR) kind = 8;
  else if (opc == OP_BXOR) kind = 9;
  else if (opc == OP_SHL) kind = 10;
  else if (opc == OP_SHR) kind = 11;
  else return 0;
  if (op_b != ra.val || op_c != rb.val)
    return 0;
  if (is_control_reg(op_a, for_a) || is_control_reg(ra.val, for_a) ||
      is_control_reg(rb.val, for_a))
    return 0;
  if (i >= backedge)
    return 0;
  mm = p->code[i++];
  if (GET_OPCODE(mm) != OP_MMBIN && GET_OPCODE(mm) != OP_MMBINI &&
      GET_OPCODE(mm) != OP_MMBINK)
    return 0;
  if (i >= backedge)
    return 0;
  set = p->code[i++];
  if (GET_OPCODE(set) != OP_SETTABLE ||
      GETARG_B(set) != for_a + 2 || GETARG_C(set) != op_a)
    return 0;
  dst = GETARG_A(set);
  if (dst == ra.val || dst == rb.val || dst == op_a ||
      is_control_reg(dst, for_a))
    return 0;
  if ((ra.upval < 0 && op_a == ra.treg) ||
      (rb.upval < 0 && op_a == rb.treg))
    return 0;
  if (ra.upval < 0 && (ra.treg == ra.val || ra.treg == rb.val))
    return 0;
  if (rb.upval < 0 && (rb.treg == rb.val || rb.treg == ra.val))
    return 0;
  if (ra.upval < 0 && !ttistable(&b[ra.table]))
    return 0;
  if (rb.upval < 0 && !ttistable(&b[rb.table]))
    return 0;
  if (!ttistable(&b[dst]) || i != backedge)
    return 0;
  return install_cdata_tint(p, J, backedge, 54, dst,
                            encode_pair_source(&ra),
                            encode_pair_source(&rb), kind,
                            (ra.val & 0xff) | ((rb.val & 0xff) << 8),
                            op_a, for_a);
}


/* LUAR: `dst[i] = -src[i]` and `dst[i] = ~src[i]` over raw table
** arrays.  Bitwise not requires integer elements; float elements stay on
** the interpreter so an exact-integral conversion is not guessed. */
static int match_table_unary_store (Proto *p, LuaR_JIT *J,
                                    ptrdiff_t start, ptrdiff_t backedge,
                                    int for_a, TValue *b) {
  LuaR_PairRead r;
  Instruction op, set;
  ptrdiff_t i = start;
  int opcode, op_a, op_b, dst;
  if (!match_pair_read(p, &i, backedge, for_a, &r))
    return 0;
  if (i >= backedge)
    return 0;
  op = p->code[i++];
  opcode = GET_OPCODE(op);
  if (opcode != OP_UNM && opcode != OP_BNOT)
    return 0;
  op_a = GETARG_A(op);
  op_b = GETARG_B(op);
  if (op_b != r.val || is_control_reg(op_a, for_a))
    return 0;
  if (i >= backedge)
    return 0;
  set = p->code[i++];
  if (GET_OPCODE(set) != OP_SETTABLE ||
      GETARG_B(set) != for_a + 2 || GETARG_C(set) != op_a)
    return 0;
  dst = GETARG_A(set);
  if (dst == r.val || dst == op_a || is_control_reg(dst, for_a) ||
      is_control_reg(r.val, for_a))
    return 0;
  if (r.upval < 0 && (r.treg == r.val || r.treg == op_a))
    return 0;
  if (r.upval < 0 && !ttistable(&b[r.table]))
    return 0;
  if (!ttistable(&b[dst]) || i != backedge)
    return 0;
  return install_table_scaled_store(p, J, backedge, for_a, dst,
                                    encode_pair_source(&r), -1, 0,
                                    r.val, op_a, opcode == OP_UNM ? 7 : 8);
}


/* LUAR: `dst[i] = src[i] +|-|*|/ k` over raw table arrays.  The source
** and destination may be the same table.  The scalar is a numeric
** constant, register local or closure upvalue. */
static int match_table_scaled_store (Proto *p, LuaR_JIT *J,
                                     ptrdiff_t start, ptrdiff_t backedge,
                                     int for_a, TValue *b) {
  LuaR_PairRead r;
  Instruction op, set;
  ptrdiff_t i = start;
  int opcode, op_a, op_b, op_c, scalar_enc = -1, scalar_reg = -1;
  int scalar_imm = 0, scalar_pre = -1, dst, kind;
  if (!match_pair_read(p, &i, backedge, for_a, &r))
    return 0;
  if (i >= backedge)
    return 0;
  /* Pure element copy: dst[i] = src[i]. */
  if (GET_OPCODE(p->code[i]) == OP_SETTABLE &&
      GETARG_B(p->code[i]) == for_a + 2 &&
      GETARG_C(p->code[i]) == r.val) {
    set = p->code[i++];
    dst = GETARG_A(set);
    if (dst == r.val || is_control_reg(dst, for_a) ||
        is_control_reg(r.val, for_a))
      return 0;
    if (r.upval < 0 && r.treg == r.val)
      return 0;
    if (r.upval < 0 && !ttistable(&b[r.table]))
      return 0;
    if (!ttistable(&b[dst]) || i != backedge)
      return 0;
    return install_cdata_tint(p, J, backedge, 57, dst,
                              encode_pair_source(&r), r.val,
                              0, 0, 0, for_a);
  }
  if (GET_OPCODE(p->code[i]) == OP_GETUPVAL) {
    Instruction up = p->code[i++];
    int sreg = GETARG_A(up);
    if (i >= backedge || sreg == r.val || is_control_reg(sreg, for_a))
      return 0;
    scalar_reg = sreg;
    scalar_pre = sreg;
    scalar_enc = -1 - GETARG_B(up);
  }
  if (i >= backedge)
    return 0;
  op = p->code[i++];
  opcode = GET_OPCODE(op);
  op_a = GETARG_A(op);
  op_b = GETARG_B(op);
  op_c = GETARG_C(op);
  if (opcode == OP_ADDI || opcode == OP_SHLI || opcode == OP_SHRI) {
    if (op_b != r.val || scalar_enc != -1)
      return 0;
    scalar_enc = LUAR_TINT_SCALAR_IMM;
    scalar_imm = (int)GETARG_sC(op);
    kind = (opcode == OP_ADDI) ? 0 : (opcode == OP_SHLI) ? 12 : 13;
  }
  else if (opcode == OP_ADDK || opcode == OP_SUBK ||
      opcode == OP_MULK || opcode == OP_DIVK ||
      opcode == OP_IDIVK || opcode == OP_MODK ||
      opcode == OP_BANDK || opcode == OP_BORK || opcode == OP_BXORK) {
    int kidx = op_c;
    if (op_b != r.val || scalar_enc != -1 || kidx >= p->sizek)
      return 0;
    if (!ttisinteger(&p->k[kidx]) && !ttisfloat(&p->k[kidx]))
      return 0;
    if (kidx >= LUAR_TINT_SCALAR_CONST)
      return 0;
    scalar_enc = LUAR_TINT_SCALAR_CONST | kidx;
    kind = (opcode == OP_ADDK) ? 0 : (opcode == OP_SUBK) ? 1
         : (opcode == OP_MULK) ? 2 : (opcode == OP_DIVK) ? 3
         : (opcode == OP_IDIVK) ? 5 : (opcode == OP_MODK) ? 6
         : (opcode == OP_BANDK) ? 9 : (opcode == OP_BORK) ? 10 : 11;
  }
  else if (opcode == OP_ADD || opcode == OP_SUB ||
           opcode == OP_MUL || opcode == OP_DIV ||
           opcode == OP_IDIV || opcode == OP_MOD ||
           opcode == OP_BAND || opcode == OP_BOR || opcode == OP_BXOR ||
           opcode == OP_SHL || opcode == OP_SHR) {
    int commutative = (opcode == OP_ADD || opcode == OP_MUL ||
                       opcode == OP_BAND || opcode == OP_BOR ||
                       opcode == OP_BXOR);
    if (op_b == r.val)
      scalar_reg = op_c;
    else if (commutative && op_c == r.val)
      scalar_reg = op_b;
    else
      return 0;
    if (scalar_pre >= 0) {
      if (scalar_reg != scalar_pre)
        return 0;
    }
    else {
      if (scalar_reg < 0 || scalar_reg > 255 ||
          is_control_reg(scalar_reg, for_a))
        return 0;
      scalar_enc = scalar_reg;
    }
    kind = (opcode == OP_ADD) ? 0 : (opcode == OP_SUB) ? 1
         : (opcode == OP_MUL) ? 2 : (opcode == OP_DIV) ? 3
         : (opcode == OP_IDIV) ? 5 : (opcode == OP_MOD) ? 6
         : (opcode == OP_BAND) ? 9 : (opcode == OP_BOR) ? 10
         : (opcode == OP_BXOR) ? 11 : (opcode == OP_SHL) ? 12 : 13;
  }
  else
    return 0;
  if (is_control_reg(op_a, for_a))
    return 0;
  if (i >= backedge ||
      (GET_OPCODE(p->code[i]) != OP_MMBIN &&
       GET_OPCODE(p->code[i]) != OP_MMBINI &&
       GET_OPCODE(p->code[i]) != OP_MMBINK))
    return 0;
  i++;
  if (i >= backedge)
    return 0;
  set = p->code[i++];
  if (GET_OPCODE(set) != OP_SETTABLE ||
      GETARG_B(set) != for_a + 2 || GETARG_C(set) != op_a)
    return 0;
  dst = GETARG_A(set);
  if (dst == r.val || dst == op_a || is_control_reg(dst, for_a) ||
      is_control_reg(r.val, for_a) || is_control_reg(scalar_reg, for_a))
    return 0;
  if (r.upval < 0 && (r.treg == r.val || r.treg == op_a))
    return 0;
  if (scalar_reg >= 0) {
    if (scalar_reg == r.val || scalar_reg == op_a || scalar_reg == dst)
      return 0;
    if (!ttisinteger(&b[scalar_reg]) && !ttisfloat(&b[scalar_reg]))
      return 0;
  }
  if (r.upval < 0 && !ttistable(&b[r.table]))
    return 0;
  if (!ttistable(&b[dst]))
    return 0;
  if (i != backedge)
    return 0;
  return install_table_scaled_store(p, J, backedge, for_a, dst,
                                    encode_pair_source(&r), scalar_enc,
                                    scalar_imm, r.val, op_a, kind);
}


/* LUAR: `s = s + a[i] * b[i]` (and its `a[i] * a[i]` square special case).
** The two reads and the MUL live in a temporary, then the accumulator gets
** the product.  Both sources may be registers or upvalues. */
static int match_dot_product (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                              ptrdiff_t backedge, int for_a, TValue *base) {
  LuaR_PairRead ra, rb;
  Instruction mul, mm1, add, mm2;
  ptrdiff_t i = start;
  int res, acc, mb, mc, ab, ac, div = 0, sub = 0, idiv = 0, mod = 0;
  if (!match_pair_read(p, &i, backedge, for_a, &ra))
    return 0;
  if (!match_pair_read(p, &i, backedge, for_a, &rb))
    return 0;
  if (ra.val == rb.val)   /* the second read would clobber the first */
    return 0;
  if (rb.treg == ra.val)  /* the upvalue load would clobber the first */
    return 0;
  if (i >= backedge)
    return 0;
  mul = p->code[i];
  if (GET_OPCODE(mul) == OP_DIV)
    div = 1;
  else if (GET_OPCODE(mul) == OP_IDIV)
    idiv = 1;
  else if (GET_OPCODE(mul) == OP_MOD)
    mod = 1;
  else if (GET_OPCODE(mul) != OP_MUL)
    return 0;
  res = GETARG_A(mul);
  mb = GETARG_B(mul);
  mc = GETARG_C(mul);
  if (div || idiv || mod) {
    /* Non-commutative arithmetic: a[i] OP b[i], left to right. */
    if (mb != ra.val || mc != rb.val)
      return 0;
  }
  else if (!((mb == ra.val && mc == rb.val) ||
             (mb == rb.val && mc == ra.val)))
    return 0;
  if (is_control_reg(res, for_a) || res == for_a + 2)
    return 0;
  i++;
  if (i >= backedge)
    return 0;
  mm1 = p->code[i];
  if (GET_OPCODE(mm1) != OP_MMBIN)
    return 0;
  i++;
  if (i >= backedge)
    return 0;
  add = p->code[i];
  acc = GETARG_A(add);
  ab = GETARG_B(add);
  ac = GETARG_C(add);
  if (GET_OPCODE(add) == OP_SUB) {
    /* acc - product; product - acc is a different reduction. */
    if (ab != acc || ac != res)
      return 0;
    sub = 1;
  }
  else if (GET_OPCODE(add) == OP_ADD) {
    if (!((ab == acc && ac == res) || (ab == res && ac == acc)))
      return 0;
  }
  else
    return 0;
  if (is_control_reg(acc, for_a) || acc == for_a + 2 || acc == res ||
      acc == ra.val || acc == rb.val ||
      ra.treg == acc || rb.treg == acc)
    return 0;
  i++;
  if (i >= backedge)
    return 0;
  mm2 = p->code[i];
  if (GET_OPCODE(mm2) != OP_MMBIN)
    return 0;
  i++;
  if (i != backedge)
    return 0;
  if (is_control_reg(ra.val, for_a) || is_control_reg(rb.val, for_a))
    return 0;
  {
    int acd = pair_cdata_source(base, &ra);
    int bcd = pair_cdata_source(base, &rb);
    if ((acd != 0) != (bcd != 0))
      return 0;
    {
      int table_kind, cdata_kind;
      if (idiv) { table_kind = sub ? 45 : 43; cdata_kind = sub ? 49 : 47; }
      else if (mod) { table_kind = sub ? 46 : 44; cdata_kind = sub ? 50 : 48; }
      else if (div) { table_kind = sub ? 40 : 36; cdata_kind = sub ? 41 : 37; }
      else { table_kind = sub ? 38 : 14; cdata_kind = sub ? 39 : 19; }
      return install_pair_intrinsic(p, J, backedge, for_a, &ra, &rb, acc,
                                    res, (acd != 0) ? cdata_kind : table_kind);
    }
  }
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
  if (!meta_multiplier(L, &base[GETARG_B(gt)], &mult) ||
      GETARG_A(gt) == GETARG_A(add))
    return 0;
  d->fast_meta = 1;
  d->fm_table = GETARG_B(gt);
  d->fm_acc = GETARG_A(add);
  d->fm_res = GETARG_A(gt);
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
  if (!meta_multiplier(L, tab, &mult) ||
      GETARG_A(gt) == GETARG_A(add))
    return 0;
  d->fast_meta = 1;
  d->fm_table = 0;
  d->fm_acc = GETARG_A(add);
  d->fm_res = GETARG_A(gt);
  d->fm_upval = GETARG_B(up);
  d->fm_mult = mult;
  return 1;
}


/* Element flavour of a constant array index at compile time.  The native
** read still guards the tag, so a later change falls back to the
** interpreter; this only chooses which specialization to emit. */
static int const_array_elem_type (const TValue *tv, int key) {
  Table *h;
  lu_byte tag;
  if (!ttistable(tv) || key < 1)
    return LUAR_T_INT;
  h = hvalue(tv);
  if ((unsigned)key - 1u >= h->asize)
    return LUAR_T_INT;
  tag = *getArrTag(h, (unsigned)key - 1u);
  return tag == LUA_VNUMFLT ? LUAR_T_FLOAT : LUAR_T_INT;
}


/* The native loop is emitted from the single static type of each register
** and does not re-check tags on every iteration.  A branch-dependent
** assignment can change a register's type between iterations (for example
** a float accumulator that becomes an integer after a threshold), which
** would make the next iteration read the new payload with the old type.
** Reject such loops instead of miscompiling them; a register may only be
** accessed as one type over the whole loop body. */
static int loop_type_check (uint8_t *types, int reg, int type) {
  if (types[reg] == 0)
    types[reg] = (uint8_t)type;
  return types[reg] == type;
}


static int loop_types_stable (const LuaR_LoopDesc *d) {
  uint8_t types[256];
  int i, k;
  memset(types, 0, sizeof(types));
  if (d->for_a >= 0) {
    int ct = d->for_float ? LUAR_T_FLOAT : LUAR_T_INT;
    for (k = 0; k < 3; k++) {
      if (!loop_type_check(types, d->for_a + k, ct))
        return 0;
    }
  }
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    int ok = 1;
    switch (op->kind) {
      case LUAR_OP_MOVE:
        ok = loop_type_check(types, op->b, op->tb) &&
             loop_type_check(types, op->a, op->tb);
        break;
      case LUAR_OP_LOADI:
        ok = loop_type_check(types, op->a, LUAR_T_INT);
        break;
      case LUAR_OP_LOADF:
      case LUAR_OP_LOADK:
        ok = loop_type_check(types, op->a, op->ta);
        break;
      case LUAR_OP_ADDI:
        ok = loop_type_check(types, op->b, LUAR_T_INT) &&
             loop_type_check(types, op->a, LUAR_T_INT);
        break;
      case LUAR_OP_ADD:
      case LUAR_OP_SUB:
      case LUAR_OP_MUL:
      case LUAR_OP_DIV:
      case LUAR_OP_MOD:
      case LUAR_OP_IDIV:
        ok = loop_type_check(types, op->b, op->tb) &&
             loop_type_check(types, op->c, op->tc) &&
             loop_type_check(types, op->a, op->ta);
        break;
      case LUAR_OP_ADDK:
      case LUAR_OP_SUBK:
      case LUAR_OP_MULK:
      case LUAR_OP_DIVK:
      case LUAR_OP_MODK:
      case LUAR_OP_IDIVK:
        ok = loop_type_check(types, op->b, op->tb) &&
             loop_type_check(types, op->a, op->ta);
        break;
      case LUAR_OP_BAND:
      case LUAR_OP_BOR:
      case LUAR_OP_BXOR:
        ok = loop_type_check(types, op->b, LUAR_T_INT) &&
             loop_type_check(types, op->c, LUAR_T_INT) &&
             loop_type_check(types, op->a, LUAR_T_INT);
        break;
      case LUAR_OP_BANDK:
      case LUAR_OP_BORK:
      case LUAR_OP_BXORK:
      case LUAR_OP_BNOT:
      case LUAR_OP_SHRI:
      case LUAR_OP_SHLI:
        ok = loop_type_check(types, op->b, LUAR_T_INT) &&
             loop_type_check(types, op->a, LUAR_T_INT);
        break;
      case LUAR_OP_UNM:
        ok = loop_type_check(types, op->b, op->tb) &&
             loop_type_check(types, op->a, op->ta);
        break;
      case LUAR_OP_SHL:
      case LUAR_OP_SHR:
        ok = loop_type_check(types, op->b, LUAR_T_INT) &&
             loop_type_check(types, op->c, LUAR_T_INT) &&
             loop_type_check(types, op->a, LUAR_T_INT);
        break;
      case LUAR_OP_GETTABLE:
        ok = loop_type_check(types, op->b, LUAR_T_TABLE) &&
             loop_type_check(types, op->c, LUAR_T_INT) &&
             loop_type_check(types, op->a, LUAR_T_INT);
        break;
      case LUAR_OP_GETI:
        ok = loop_type_check(types, op->b, LUAR_T_TABLE) &&
             loop_type_check(types, op->a, op->ta);
        break;
      case LUAR_OP_TABLE_ADDI:
        ok = loop_type_check(types, op->b, LUAR_T_TABLE) &&
             loop_type_check(types, op->a, op->ta) &&
             loop_type_check(types, op->res, op->elem);
        break;
      case LUAR_OP_TABLE_ADD:
        ok = loop_type_check(types, op->b, LUAR_T_TABLE) &&
             loop_type_check(types, op->c, LUAR_T_INT) &&
             loop_type_check(types, op->a, op->ta) &&
             loop_type_check(types, op->res, LUAR_T_INT);
        break;
      case LUAR_OP_SETTABLE:
        ok = loop_type_check(types, op->a, LUAR_T_TABLE) &&
             loop_type_check(types, op->b, LUAR_T_INT);
        if (ok && !op->isconst)
          ok = loop_type_check(types, op->c, op->ta);
        break;
      case LUAR_OP_SETI:
        ok = loop_type_check(types, op->a, LUAR_T_TABLE);
        if (ok && !op->isconst)
          ok = loop_type_check(types, op->c, op->ta);
        break;
      case LUAR_OP_LT:
      case LUAR_OP_LE:
      case LUAR_OP_GT:
      case LUAR_OP_GE:
      case LUAR_OP_EQ:
        ok = loop_type_check(types, op->a, op->ta);
        if (ok && op->b != 0xFF)
          ok = loop_type_check(types, op->b, op->tb);
        break;
      default:
        break;
    }
    if (!ok)
      return 0;
  }
  return 1;
}


/* The leaf-call intrinsics snapshot their extra arguments before running the
** remaining iterations, while the bytecode re-reads those registers on every
** call.  A loop such as `s = s + f(i, i)` therefore used the first index for
** every recursion of the helper.  Reject a leaf fast path when an argument
** source register is written anywhere in the loop body (including the
** control triplet, the accumulator and the call result range). */
static int leaf_args_invariant (Proto *p, ptrdiff_t start,
                                ptrdiff_t backedge, int for_a,
                                const Instruction *moves, int nmoves) {
  uint8_t written[256];
  ptrdiff_t i;
  int m;
  memset(written, 0, sizeof(written));
  if (for_a >= 0 && for_a + 2 < 256) {
    written[for_a] = written[for_a + 1] = written[for_a + 2] = 1;
  }
  for (i = start; i < backedge; i++) {
    Instruction ins = p->code[i];
    switch (GET_OPCODE(ins)) {
      case OP_MOVE:
      case OP_LOADI:
      case OP_LOADF:
      case OP_LOADK:
      case OP_ADDI:
      case OP_ADD:
      case OP_SUB:
      case OP_MUL:
      case OP_DIV:
        written[GETARG_A(ins)] = 1;
        break;
      case OP_CALL: {
        int a = GETARG_A(ins);
        int b = GETARG_B(ins);
        int c = GETARG_C(ins);
        int n = (b > c) ? b : c;
        int r;
        for (r = a; r < a + n && r < 256; r++)
          written[r] = 1;
        break;
      }
      case OP_FORLOOP:
        written[GETARG_A(ins)] = 1;
        written[GETARG_A(ins) + 1] = 1;
        written[GETARG_A(ins) + 2] = 1;
        break;
      default:
        break;
    }
  }
  for (m = 0; m < nmoves; m++) {
    int src = GETARG_B(moves[m]);
    if (src >= 0 && src < 256 && written[src])
      return 0;
  }
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
  int field_writes = 0;
  int for_float = 0;
  int call_at = -1;
  if (!luari_emit_available() || b == NULL || n < 2 || n > LUAR_JIT_MAXINS)
    return 0;
  /* Drop slots left over from a previous epoch so a flushed Proto can use
  ** both slots for fresh loops. */
  for (i = 0; i < 2; i++) {
    if (p->luar_code[i] != NULL && p->luar_code_epoch[i] != J->epoch)
      p->luar_code[i] = NULL;
    if (p->luar_tint_epoch[i] != J->epoch || p->luar_tint[i] == 0) {
      if (p->luar_tint_epoch[i] != J->epoch)
        p->luar_tint[i] = 0;
      p->luar_tint_suspend[i] = 0;
    }
  }
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
    if (ttisinteger(&b[for_a]) && ttisinteger(&b[for_a + 1]) &&
        ttisinteger(&b[for_a + 2]))
      for_float = 0;
    else if (ttisfloat(&b[for_a]) && ttisfloat(&b[for_a + 1]) &&
             ttisfloat(&b[for_a + 2]))
      for_float = 1;
    else
      return 0;   /* mixed/non-numeric loop control */
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
    if (!for_float && J->enable_tint && n >= 3 && GET_OPCODE(i0) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 1]) == OP_SETTABLE &&
        backedge == start + 2) {
      Instruction i1 = p->code[start + 1];
      int kt = GETARG_A(i0);
      if (GETARG_C(i0) == for_a + 2 &&
          GETARG_B(i1) == kt &&
          GETARG_C(i1) == for_a + 2 &&
          kt != GETARG_A(i1) && kt != GETARG_B(i0) &&
          !is_control_reg(kt, for_a))
        return install_keyed_fill(p, J, backedge, for_a,
                                  GETARG_A(i1), GETARG_B(i0), -1, kt);
    }
    if (!for_float && J->enable_tint && n >= 4 && GET_OPCODE(i0) == OP_GETUPVAL &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 2]) == OP_SETTABLE &&
        backedge == start + 3) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      int kt = GETARG_A(i1);
      if (GETARG_A(i0) == GETARG_B(i1) &&
          GETARG_C(i1) == for_a + 2 &&
          GETARG_B(i2) == kt &&
          GETARG_C(i2) == for_a + 2 &&
          kt != GETARG_A(i2) &&
          !is_control_reg(kt, for_a))
        return install_keyed_fill(p, J, backedge, for_a,
                                  GETARG_A(i2), 0, GETARG_B(i0), kt);
    }
    if (!for_float && J->enable_tint && n >= 5 && GET_OPCODE(i0) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 2]) == OP_ADD &&
        GET_OPCODE(p->code[start + 3]) == OP_MMBIN &&
        backedge == start + 4) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      {
        int kv = GETARG_A(i0);
        if (GETARG_C(i0) == for_a + 2 &&
            GETARG_C(i1) == kv &&
            GETARG_A(i1) == kv &&
            GETARG_A(i2) == GETARG_B(i2) &&
            GETARG_C(i2) == kv &&
            kv != GETARG_B(i0) && kv != GETARG_B(i1) &&
            kv != GETARG_A(i2) &&
            !is_control_reg(kv, for_a))
          return install_keyed_sum(p, J, backedge, for_a,
                                   GETARG_B(i1), GETARG_B(i0), -1,
                                   GETARG_A(i2), kv);
      }
    }
    if (!for_float && J->enable_tint && n >= 5 &&
        GET_OPCODE(i0) == OP_GETUPVAL &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        (GET_OPCODE(p->code[start + 2]) == OP_ADD ||
         GET_OPCODE(p->code[start + 2]) == OP_SUB ||
         GET_OPCODE(p->code[start + 2]) == OP_MUL ||
         GET_OPCODE(p->code[start + 2]) == OP_BAND ||
         GET_OPCODE(p->code[start + 2]) == OP_BOR ||
         GET_OPCODE(p->code[start + 2]) == OP_BXOR ||
         GET_OPCODE(p->code[start + 2]) == OP_SHL ||
         GET_OPCODE(p->code[start + 2]) == OP_SHR ||
         GET_OPCODE(p->code[start + 2]) == OP_IDIV ||
         GET_OPCODE(p->code[start + 2]) == OP_MOD) &&
        GET_OPCODE(p->code[start + 3]) == OP_MMBIN &&
        backedge == start + 4) {
      /* LUAR: `s = s +/- a[i]` where `a` is a bounded cdata array captured
      ** as a closure upvalue.  The upvalue is re-resolved on every entry,
      ** so reassigning it is observed like the interpreter. */
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      int treg = GETARG_A(i0), tmp = GETARG_A(i1), arr = GETARG_B(i1);
      int acc = GETARG_A(i2);
      TValue *av = pair_source_value((TValue *)b, -1, GETARG_B(i0));
      if (GETARG_A(i1) == treg && arr == treg &&
          GETARG_C(i1) == for_a + 2 &&
          GETARG_A(i2) == GETARG_B(i2) &&
          GETARG_C(i2) == tmp &&
          tmp != acc && arr != acc &&
          !is_control_reg(tmp, for_a) &&
          !is_control_reg(acc, for_a) &&
          !is_control_reg(arr, for_a) &&
          av != NULL &&
          (luar_ffi_trace_array_kind(av) != 0 ||
           (luar_ffi_trace_array_kind64(av) != 0 &&
            luar_ffi_trace_i64_scalar(&b[acc]))))
        return install_cdata_tint(p, J, backedge,
                                  luar_ffi_trace_array_kind64(av) != 0 ? 69 : 16,
                                  -1 - GETARG_B(i0), 0, 0, acc, tmp,
                                  (GET_OPCODE(i2) == OP_MUL) ? 2 :
                                  (GET_OPCODE(i2) == OP_SUB) ? 1 :
                                  (GET_OPCODE(i2) == OP_BAND) ? 3 :
                                  (GET_OPCODE(i2) == OP_BOR) ? 4 :
                                  (GET_OPCODE(i2) == OP_BXOR) ? 5 :
                                  (GET_OPCODE(i2) == OP_SHL) ? 6 :
                                  (GET_OPCODE(i2) == OP_SHR) ? 7 :
                                  (GET_OPCODE(i2) == OP_IDIV) ? 8 :
                                  (GET_OPCODE(i2) == OP_MOD) ? 9 : 0, for_a);
    }
    if (!for_float && J->enable_tint && n >= 5 &&
        GET_OPCODE(i0) == OP_GETUPVAL &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 2]) == OP_ADD &&
        GET_OPCODE(p->code[start + 3]) == OP_MMBIN &&
        backedge == start + 4) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      int treg = GETARG_A(i0), rreg = GETARG_A(i1), areg = GETARG_A(i2);
      if (GETARG_B(i1) == treg &&
          GETARG_C(i1) == for_a + 2 &&
          GETARG_C(i2) == rreg &&
          areg == GETARG_B(i2) &&
          treg != areg && rreg != areg &&
          !is_control_reg(rreg, for_a) &&
          !is_control_reg(treg, for_a))
        return install_upval_sum(p, J, backedge, for_a,
                                 GETARG_B(i0), areg, rreg);
    }
    if (!for_float && J->enable_tint && n >= 6 && GET_OPCODE(i0) == OP_GETUPVAL &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 2]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 3]) == OP_ADD &&
        GET_OPCODE(p->code[start + 4]) == OP_MMBIN &&
        backedge == start + 5) {
      Instruction i1 = p->code[start + 1];
      Instruction i2 = p->code[start + 2];
      Instruction i3 = p->code[start + 3];
      {
        int kv = GETARG_A(i1);
        if (GETARG_A(i0) == GETARG_B(i1) &&
            GETARG_C(i1) == for_a + 2 &&
            GETARG_C(i2) == kv &&
            GETARG_A(i2) == kv &&
            GETARG_A(i3) == GETARG_B(i3) &&
            GETARG_C(i3) == kv &&
            kv != GETARG_B(i2) && kv != GETARG_A(i3) &&
            !is_control_reg(kv, for_a))
          return install_keyed_sum(p, J, backedge, for_a,
                                   GETARG_B(i2), 0, GETARG_B(i0),
                                   GETARG_A(i3), kv);
      }
    }
    if (!for_float && J->enable_tint && n >= 4 &&
        GET_OPCODE(i0) == OP_GETTABLE &&
        (GET_OPCODE(p->code[start + 1]) == OP_ADD ||
         GET_OPCODE(p->code[start + 1]) == OP_SUB ||
         GET_OPCODE(p->code[start + 1]) == OP_MUL ||
         GET_OPCODE(p->code[start + 1]) == OP_BAND ||
         GET_OPCODE(p->code[start + 1]) == OP_BOR ||
         GET_OPCODE(p->code[start + 1]) == OP_BXOR ||
         GET_OPCODE(p->code[start + 1]) == OP_SHL ||
         GET_OPCODE(p->code[start + 1]) == OP_SHR ||
         GET_OPCODE(p->code[start + 1]) == OP_IDIV ||
         GET_OPCODE(p->code[start + 1]) == OP_MOD) &&
        GET_OPCODE(p->code[start + 2]) == OP_MMBIN &&
        backedge == start + 3) {
      /* LUAR: `s = s +/- a[i]` over a bounded FFI cdata array.  The generic
      ** decoder only knows Lua tables, so recognise the raw GETTABLE shape
      ** here and let a C helper run the remaining iterations. */
      Instruction i1 = p->code[start + 1];
      int tmp = GETARG_A(i0), arr = GETARG_B(i0);
      int acc = GETARG_A(i1);
      if (GETARG_C(i0) == for_a + 2 &&
          GETARG_A(i1) == GETARG_B(i1) &&
          GETARG_C(i1) == tmp &&
          tmp != acc && arr != acc && arr != tmp &&
          !is_control_reg(tmp, for_a) &&
          !is_control_reg(acc, for_a) &&
          !is_control_reg(arr, for_a) &&
          (luar_ffi_trace_array_kind(&b[arr]) != 0 ||
           (luar_ffi_trace_array_kind64(&b[arr]) != 0 &&
            luar_ffi_trace_i64_scalar(&b[acc]))))
        return install_cdata_tint(p, J, backedge,
                                  luar_ffi_trace_array_kind64(&b[arr]) != 0 ? 68 : 16,
                                  arr, 0, 0, acc, tmp,
                                  (GET_OPCODE(i1) == OP_MUL) ? 2 :
                                  (GET_OPCODE(i1) == OP_SUB) ? 1 :
                                  (GET_OPCODE(i1) == OP_BAND) ? 3 :
                                  (GET_OPCODE(i1) == OP_BOR) ? 4 :
                                  (GET_OPCODE(i1) == OP_BXOR) ? 5 :
                                  (GET_OPCODE(i1) == OP_SHL) ? 6 :
                                  (GET_OPCODE(i1) == OP_SHR) ? 7 :
                                  (GET_OPCODE(i1) == OP_IDIV) ? 8 :
                                  (GET_OPCODE(i1) == OP_MOD) ? 9 : 0, for_a);
    }
    if (!for_float && J->enable_tint &&
        match_three_table_sum(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_pair_sum(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_dot_product(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_scaled_sum(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_scaled_store(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_affine_scalar_fill(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_affine_fill(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_upval_affine_fill(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_upval_index_fill(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_any_fill(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_nil_store(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_upval_nil(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_upval_copy(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_upval_body(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_unary_store(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_table_pair_store(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_fill(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_update(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_field_table_sum(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_array_fill_f64_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_array_copy_from_upval_f64(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_array_copy_f64(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_array_fill_f64_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_array_fill_f64(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_array_fill_f64(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_array_update_f64_number(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_array_update_f64_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_fill_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_field_tabup_f64_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_field_tabup_f64_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_update_f64_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_update_f64_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_fill_f64_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_update_f32_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_update_f32_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_fill_f32_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_fill_f32_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_fill_f64_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_fill_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_copy_f32_upval(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_copy_f64_upval(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_copy_upval(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_copy_float_mixed(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_copy_f32(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_copy_f64(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_copy(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_field_sum(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_sum(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_field_tabup_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_field_tabup_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_field_update_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_update_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_field_update_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_update_scalar(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_update_const(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_upval_body(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_unary_store(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_pair_store(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_store(p, J, start, backedge, for_a, b))
      return 1;
    if (!for_float && J->enable_tint &&
        match_cdata_scalar_store(p, J, start, backedge, for_a, b))
      return 1;
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
          GETARG_C(i6) == GETARG_A(i5)) {
        if (!leaf_args_invariant(p, start, backedge, for_a,
                                 &p->code[start + 2], 3))
          return 0;
        return install_leaf4_intrinsic(p, J, backedge, for_a,
                                       GETARG_B(i0), GETARG_A(i6),
                                       GETARG_A(i2), GETARG_A(i3),
                                       GETARG_A(i4));
      }
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
          GETARG_C(i5) == GETARG_A(i4)) {
        if (!leaf_args_invariant(p, start, backedge, for_a,
                                 &p->code[start + 2], 2))
          return 0;
        return install_leaf3_intrinsic(p, J, backedge, for_a,
                                       GETARG_B(i0), GETARG_A(i5),
                                       GETARG_A(i2), GETARG_A(i3));
      }
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
          GETARG_C(i4) == GETARG_A(i3)) {
        if (!leaf_args_invariant(p, start, backedge, for_a,
                                 &p->code[start + 2], 1))
          return 0;
        return install_leaf2_intrinsic(p, J, backedge, for_a,
                                       GETARG_B(i0), GETARG_A(i4),
                                       GETARG_A(i2));
      }
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
  /* A folded field read is only valid if no supported instruction in the
  ** body can mutate a table.  Calls are rejected by the decoder anyway. */
  for (i = 0; i < n - 1; i++) {
    switch (GET_OPCODE(p->code[start + i])) {
      case OP_SETTABLE: case OP_SETI: case OP_SETFIELD:
      case OP_SETTABUP: case OP_NEWTABLE: case OP_SETLIST:
        field_writes = 1;
        break;
      default:
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
  d.tag_table = ctb(LUA_VTABLE);   /* collectable bit is part of the tag */
  d.table_array_off = (int)offsetof(Table, array);
  d.table_asize_off = (int)offsetof(Table, asize);
  d.table_metatable_off = (int)offsetof(Table, metatable);
  d.for_float = for_float;
  if (!is_while) {
    int ct = for_float ? LUAR_T_FLOAT : LUAR_T_INT;
    for (i = 0; i < 3; i++) {
      cur[for_a + i] = (uint8_t)ct;
      def[for_a + i] = 1;
      add_guard(&d, seen, for_a + i, ct);
    }
  }

  if (!is_while && !for_float && for_a >= 0 &&
      match_meta_upval_loop(L, p, start, backedge, for_a, b, &d))
    goto emit_ready;
  if (!is_while && !for_float && for_a >= 0 &&
      match_meta_loop(L, p, start, backedge, for_a, b, &d))
    goto emit_ready;
  if (!is_while && !for_float && for_a >= 0 &&
      match_closure_loop(p, start, backedge, for_a, b, &d))
    goto emit_ready;
  for (i = 0; i < n - 1; i++) {
    Instruction ins = p->code[start + i];
    LuaR_Op *op = &ops[i];
    int tb, tc;
    OpCode opc = GET_OPCODE(ins);
    memset(op, 0, sizeof(*op));
    op->res = 0xFF;
    op->pc = (int)(start + i);
    if (opc == OP_TEST || opc == OP_TESTSET) {
      Instruction jins;
      ptrdiff_t j = i + 1, tgt;
      int aa = GETARG_A(ins), bb = GETARG_B(ins);
      int kk = GETARG_k(ins);
      int at;
      if (j >= n - 1) return 0;
      jins = p->code[start + j];
      if (GET_OPCODE(jins) != OP_JMP) return 0;
      tgt = (start + j) + 1 + GETARG_sJ(jins);
      if (tgt < start || tgt == start + j ||
          (tgt <= start + i && tgt != start))
        return 0;
      if (tgt > backedge) {
        if (tgt != backedge + 1) return 0;
        tgt = is_while ? backedge : backedge + 1;   /* break/exit */
      }
      if (opc == OP_TEST) {
        /* A number or table is always truthy, so the test itself has no
        ** effect.  Keep a self-MOVE so the branch guard pass still checks
        ** the register's entry type: a later call with false/nil must not
        ** enter native code that assumed truthiness. */
        at = slot_type(b, cur, def, aa);
        if (at == 0) return 0;
        memset(&ops[j], 0, sizeof(ops[j]));
        if (is_control_reg(aa, for_a)) {
          /* The loop-control guard already covers its type. */
          op->kind = LUAR_OP_NOP;
        }
        else {
          op->kind = LUAR_OP_MOVE;
          op->a = (uint8_t)aa;
          op->b = (uint8_t)aa;
          op->tb = (uint8_t)at;
        }
        if (kk == 0) {
          /* A truthy value skips the following JMP. */
          ops[j].kind = LUAR_OP_NOP;
        }
        else {
          ops[j].kind = LUAR_OP_JMP;
          ops[j].target = (int32_t)(tgt - start);
        }
      }
      else {
        if (is_control_reg(aa, for_a)) return 0;
        at = slot_type(b, cur, def, bb);
        if (at == 0) return 0;
        memset(&ops[j], 0, sizeof(ops[j]));
        if (kk == 0) {
          /* Falsy skips the JMP and leaves R[A] alone.  A numeric/table
          ** value is truthy, so only the type guard is needed.  Control
          ** registers already have one. */
          if (is_control_reg(bb, for_a))
            op->kind = LUAR_OP_NOP;
          else {
            op->kind = LUAR_OP_MOVE;
            op->a = (uint8_t)bb;
            op->b = (uint8_t)bb;
            op->tb = (uint8_t)at;
          }
          ops[j].kind = LUAR_OP_NOP;
        }
        else {
          if (at != LUAR_T_INT && at != LUAR_T_FLOAT) return 0;
          op->kind = LUAR_OP_MOVE;
          op->a = (uint8_t)aa;
          op->b = (uint8_t)bb;
          op->tb = (uint8_t)at;
          cur[aa] = (uint8_t)at;
          def[aa] = 1;
          ops[j].kind = LUAR_OP_JMP;
          ops[j].target = (int32_t)(tgt - start);
        }
      }
      paired[j] = 1;
      has_branch = 1;
      i = j;
      continue;
    }
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
      case OP_BAND:
      case OP_BOR:
      case OP_BXOR: {
        int ka = GETARG_A(ins);
        if (is_control_reg(ka, for_a)) return 0;
        tb = slot_type(b, cur, def, GETARG_B(ins));
        tc = slot_type(b, cur, def, GETARG_C(ins));
        if (tb != LUAR_T_INT || tc != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        add_guard(&d, seen, GETARG_C(ins), LUAR_T_INT);
        op->kind = GET_OPCODE(ins) == OP_BAND ? LUAR_OP_BAND :
                   GET_OPCODE(ins) == OP_BOR ? LUAR_OP_BOR : LUAR_OP_BXOR;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->c = (uint8_t)GETARG_C(ins);
        op->ta = op->tb = op->tc = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      }
      case OP_BANDK:
      case OP_BORK:
      case OP_BXORK: {
        int ka = GETARG_A(ins);
        int idx = (int)GETARG_C(ins);
        TValue *k;
        if (is_control_reg(ka, for_a)) return 0;
        if (idx < 0 || idx >= p->sizek) return 0;
        k = &p->k[idx];
        if (!ttisinteger(k)) return 0;
        if (slot_type(b, cur, def, GETARG_B(ins)) != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        op->kind = GET_OPCODE(ins) == OP_BANDK ? LUAR_OP_BANDK :
                   GET_OPCODE(ins) == OP_BORK ? LUAR_OP_BORK :
                                               LUAR_OP_BXORK;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->imm = (int64_t)ivalue(k);
        op->ta = op->tb = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      }
      case OP_BNOT:
        if (is_control_reg(GETARG_A(ins), for_a)) return 0;
        if (slot_type(b, cur, def, GETARG_B(ins)) != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        op->kind = LUAR_OP_BNOT;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        op->ta = op->tb = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      case OP_UNM: {
        int ka = GETARG_A(ins);
        if (is_control_reg(ka, for_a)) return 0;
        tb = slot_type(b, cur, def, GETARG_B(ins));
        if (tb != LUAR_T_INT && tb != LUAR_T_FLOAT) return 0;
        add_guard(&d, seen, GETARG_B(ins), tb);
        op->kind = LUAR_OP_UNM;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->ta = op->tb = (uint8_t)tb;
        cur[op->a] = (uint8_t)tb;
        def[op->a] = 1;
        break;
      }
      case OP_SHRI:
        if (is_control_reg(GETARG_A(ins), for_a)) return 0;
        if (slot_type(b, cur, def, GETARG_B(ins)) != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        op->kind = LUAR_OP_SHRI;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        op->imm = (int64_t)GETARG_sC(ins);
        op->ta = op->tb = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      case OP_SHLI: {
        int ka = GETARG_A(ins);
        if (is_control_reg(ka, for_a)) return 0;
        if (slot_type(b, cur, def, GETARG_B(ins)) != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        op->kind = LUAR_OP_SHLI;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->imm = (int64_t)GETARG_sC(ins);
        op->ta = op->tb = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      }
      case OP_SHL:
      case OP_SHR: {
        int ka = GETARG_A(ins);
        if (is_control_reg(ka, for_a)) return 0;
        tb = slot_type(b, cur, def, GETARG_B(ins));
        tc = slot_type(b, cur, def, GETARG_C(ins));
        if (tb != LUAR_T_INT || tc != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        add_guard(&d, seen, GETARG_C(ins), LUAR_T_INT);
        op->kind = GET_OPCODE(ins) == OP_SHL ? LUAR_OP_SHL : LUAR_OP_SHR;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->c = (uint8_t)GETARG_C(ins);
        op->ta = op->tb = op->tc = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      }
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
      case OP_MOD:
      case OP_IDIV: {
        int ka = GETARG_A(ins);
        if (is_control_reg(ka, for_a)) return 0;
        tb = slot_type(b, cur, def, GETARG_B(ins));
        tc = slot_type(b, cur, def, GETARG_C(ins));
        if (tb != LUAR_T_INT || tc != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        add_guard(&d, seen, GETARG_C(ins), LUAR_T_INT);
        op->kind = GET_OPCODE(ins) == OP_MOD ? LUAR_OP_MOD : LUAR_OP_IDIV;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->c = (uint8_t)GETARG_C(ins);
        op->ta = op->tb = op->tc = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
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
      case OP_MODK:
      case OP_IDIVK: {
        int ka = GETARG_A(ins);
        int idx = (int)GETARG_C(ins);
        TValue *k;
        int64_t kv;
        if (is_control_reg(ka, for_a)) return 0;
        if (idx < 0 || idx >= p->sizek) return 0;
        k = &p->k[idx];
        if (!ttisinteger(k)) return 0;
        kv = (int64_t)ivalue(k);
        /* Division by zero must still raise, and -1 has the INT_MIN
        ** overflow special case; leave both to the interpreter. */
        if (kv == 0 || kv == -1) return 0;
        if (slot_type(b, cur, def, GETARG_B(ins)) != LUAR_T_INT) return 0;
        add_guard(&d, seen, GETARG_B(ins), LUAR_T_INT);
        op->kind = GET_OPCODE(ins) == OP_MODK ? LUAR_OP_MODK
                                             : LUAR_OP_IDIVK;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)GETARG_B(ins);
        op->imm = kv;
        op->ta = op->tb = LUAR_T_INT;
        cur[op->a] = LUAR_T_INT;
        def[op->a] = 1;
        break;
      }
      case OP_GETI: {
        int ka = GETARG_A(ins), kb = GETARG_B(ins);
        int kidx = (int)GETARG_C(ins);
        if (d.table_reads >= 2) return 0;   /* at most two raw reads */
        if (!d.has_table) {
          d.has_table = 1;
          d.table_reg = kb;
        }
        else if (d.table_reg != kb) {
          if (d.has_table2) return 0;
          d.has_table2 = 1;
          d.table_reg2 = kb;
        }
        if (kidx < 1) return 0;                /* t[0] belongs to the hash part */
        if (is_control_reg(ka, for_a)) return 0;
        if (slot_type(b, cur, def, kb) != LUAR_T_TABLE) return 0;
        {
          int et = const_array_elem_type(&b[kb], kidx);
          op->kind = LUAR_OP_GETI;
          op->a = (uint8_t)ka;
          op->b = (uint8_t)kb;
          op->imm = kidx;
          op->ta = (uint8_t)et;
          op->elem = (uint8_t)et;
          op->tb = LUAR_T_TABLE;
          cur[ka] = (uint8_t)et;
          def[ka] = 1;
        }
        d.table_reads++;
        d.ft_res = ka;
        has_branch = 1;

        /* Fuse the common "acc = acc + t[const]" shape.  A second,
        ** distinct table uses its own cached array pointer, so its read is
        ** not folded into the first table's fused add. */
        if (!d.has_table2 && i + 1 < n - 1) {
          Instruction nx = p->code[start + i + 1];
          if (GET_OPCODE(nx) == OP_ADD) {
            int na = GETARG_A(nx), nb = GETARG_B(nx), nc = GETARG_C(nx);
            int acct = 0;
            if ((nb == ka && nc == na) || (nc == ka && nb == na))
              acct = slot_type(b, cur, def, na);
            if ((acct == LUAR_T_INT || acct == LUAR_T_FLOAT) &&
                !is_control_reg(na, for_a) && ka != na) {
              int rt = (acct == LUAR_T_FLOAT || op->elem == LUAR_T_FLOAT)
                       ? LUAR_T_FLOAT : LUAR_T_INT;
              /* The interpreter writes the element to R[ka] before the
              ** add.  Keep that result live: a local assigned from t[i]
              ** can be read after the loop or later in the body. */
              op->kind = LUAR_OP_TABLE_ADDI;
              op->a = (uint8_t)na;
              op->b = (uint8_t)kb;
              op->res = (uint8_t)ka;
              op->ta = (uint8_t)rt;
              op->tb = LUAR_T_TABLE;
              memset(&ops[i + 1], 0, sizeof(ops[i + 1]));
              ops[i + 1].kind = LUAR_OP_NOP;
              cur[na] = (uint8_t)rt;
              def[na] = 1;
              i++;
            }
          }
        }
        break;
      }
      case OP_GETTABLE: {
        int ka = GETARG_A(ins), kb = GETARG_B(ins), kc = GETARG_C(ins);
        if (d.table_reads >= 2) return 0;   /* at most two raw reads */
        if (!d.has_table) {
          d.has_table = 1;
          d.table_reg = kb;
        }
        else if (d.table_reg != kb) {
          if (d.has_table2) return 0;
          d.has_table2 = 1;
          d.table_reg2 = kb;
        }
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
        d.table_reads++;
        d.ft_res = ka;   /* the bytecode also writes the element here */
        has_branch = 1;   /* use the labelled emitter and typed guards */
        /* Fuse the common "acc = acc + t[key]" shape.  See GETI above. */
        if (!d.has_table2 && i + 1 < n - 1) {
          Instruction nx = p->code[start + i + 1];
          if (GET_OPCODE(nx) == OP_ADD || GET_OPCODE(nx) == OP_SUB) {
            int na = GETARG_A(nx), nb = GETARG_B(nx), nc = GETARG_C(nx);
            int acct = 0, is_sub = 0;
            if (GET_OPCODE(nx) == OP_SUB) {
              /* acc - t[key]; the reverse order is not this shape.  Only
              ** fuse when this read is the whole body (the C helper shape);
              ** in a branch the generic emitter must see the real SUB. */
              if (nb == na && nc == ka && d.table_reads == 1 &&
                  start + i + 3 == backedge) {
                acct = slot_type(b, cur, def, na);
                is_sub = 1;
              }
            }
            else if ((nb == ka && nc == na) || (nc == ka && nb == na))
              acct = slot_type(b, cur, def, na);
            if ((acct == LUAR_T_INT || acct == LUAR_T_FLOAT) &&
                !is_control_reg(na, for_a) && ka != na) {
              /* R[ka] is part of the arithmetic's bytecode effect;
              ** materialise it so later reads and the post-loop value stay
              ** correct. */
              op->kind = LUAR_OP_TABLE_ADD;
              op->a = (uint8_t)na;
              op->b = (uint8_t)kb;
              op->c = (uint8_t)kc;
              op->res = (uint8_t)ka;
              op->ta = (uint8_t)acct;
              op->tb = LUAR_T_TABLE;
              op->tc = LUAR_T_INT;
              op->elem = LUAR_T_INT;
              op->is_sub = (uint8_t)is_sub;
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
      case OP_GETFIELD: {
        int ka = GETARG_A(ins), kb = GETARG_B(ins), kc = GETARG_C(ins);
        TValue *tv, field;
        Table *h;
        if (field_writes || d.fast_field) return 0;
        if (ka == kb || is_control_reg(ka, for_a)) return 0;
        if (kc < 0 || kc >= p->sizek ||
            !ttisstring(&p->k[kc])) return 0;
        if (slot_type(b, cur, def, kb) != LUAR_T_TABLE) return 0;
        tv = &b[kb];
        if (!ttistable(tv)) return 0;
        h = hvalue(tv);
        memset(&field, 0, sizeof(field));
        if (luaH_getstr(h, tsvalue(&p->k[kc]), &field) == LUA_TNIL)
          return 0;
        add_guard(&d, seen, kb, LUAR_T_TABLE);
        op->a = (uint8_t)ka;
        op->b = (uint8_t)kb;
        if (ttisinteger(&field)) {
          op->kind = LUAR_OP_LOADI;
          op->imm = (int64_t)ivalue(&field);
          op->ta = LUAR_T_INT;
          d.ffld_imm = op->imm;
        }
        else if (ttisfloat(&field)) {
          op->kind = LUAR_OP_LOADF;
          op->num = (double)fltvalue(&field);
          op->ta = LUAR_T_FLOAT;
          d.ffld_num = op->num;
        }
        else
          return 0;
        d.fast_field = 1;
        d.ffld_table = kb;
        d.ffld_key = kc;
        d.ffld_type = op->ta;
        cur[ka] = op->ta;
        def[ka] = 1;
        break;
      }
      case OP_SETTABLE: {
        int ka = GETARG_A(ins), kb = GETARG_B(ins), kc = GETARG_C(ins);
        int vt;
        if (d.table_write || d.table_reads >= 2) return 0;
        if (d.has_table && d.table_reg != ka) return 0;
        if (slot_type(b, cur, def, ka) != LUAR_T_TABLE) return 0;
        if (slot_type(b, cur, def, kb) != LUAR_T_INT) return 0;
        if (GETARG_k(ins)) {
          /* RK(C) is a constant; reading R[C] instead would store whatever
          ** happens to live in that register.  Only numeric constants can
          ** be emitted natively. */
          TValue *k;
          if (kc < 0 || kc >= p->sizek) return 0;
          k = &p->k[kc];
          if (ttisinteger(k)) {
            vt = LUAR_T_INT;
            op->imm = (int64_t)ivalue(k);
          }
          else if (ttisfloat(k)) {
            vt = LUAR_T_FLOAT;
            op->num = (double)fltvalue(k);
          }
          else
            return 0;
          op->isconst = 1;
          op->c = 0;
        }
        else {
          vt = slot_type(b, cur, def, kc);
          if (vt != LUAR_T_INT && vt != LUAR_T_FLOAT) return 0;
          op->c = (uint8_t)kc;
        }
        op->kind = LUAR_OP_SETTABLE;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)kb;
        op->ta = (uint8_t)vt;
        op->tb = LUAR_T_TABLE;
        op->tc = LUAR_T_INT;
        if (!d.has_table) {
          d.has_table = 1;
          d.table_reg = ka;
        }
        d.table_write = 1;
        has_branch = 1;
        break;
      }
      case OP_SETI: {
        int ka = GETARG_A(ins), kb = GETARG_B(ins), kc = GETARG_C(ins);
        int vt;
        if (d.table_write || d.table_reads >= 2) return 0;
        if (d.has_table && d.table_reg != ka) return 0;
        if (kb < 1) return 0;        /* t[0] belongs to the hash part */
        if (slot_type(b, cur, def, ka) != LUAR_T_TABLE) return 0;
        if (GETARG_k(ins)) {
          TValue *k;
          if (kc < 0 || kc >= p->sizek) return 0;
          k = &p->k[kc];
          if (ttisinteger(k)) {
            vt = LUAR_T_INT;
            op->imm = (int64_t)ivalue(k);
          }
          else if (ttisfloat(k)) {
            vt = LUAR_T_FLOAT;
            op->num = (double)fltvalue(k);
          }
          else
            return 0;
          op->isconst = 1;
          op->c = 0;
        }
        else {
          vt = slot_type(b, cur, def, kc);
          if (vt != LUAR_T_INT && vt != LUAR_T_FLOAT) return 0;
          op->c = (uint8_t)kc;
        }
        op->kind = LUAR_OP_SETI;
        op->a = (uint8_t)ka;
        op->b = (uint8_t)kb;       /* immediate array key */
        op->keyimm = kb;
        op->ta = (uint8_t)vt;
        op->tb = LUAR_T_TABLE;
        if (!d.has_table) {
          d.has_table = 1;
          d.table_reg = ka;
        }
        d.table_write = 1;
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
        op->kind == LUAR_OP_MUL || op->kind == LUAR_OP_DIV ||
        op->kind == LUAR_OP_MOD || op->kind == LUAR_OP_IDIV) {
      if (op->a == op->b && op->ta != op->tb) return 0;
      if (op->a == op->c && op->ta != op->tc) return 0;
    }
    else if (op->kind == LUAR_OP_ADDK || op->kind == LUAR_OP_SUBK ||
             op->kind == LUAR_OP_MULK || op->kind == LUAR_OP_DIVK ||
             op->kind == LUAR_OP_MODK || op->kind == LUAR_OP_IDIVK) {
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
  if (!for_float && !has_branch && !is_while && n == 3) {
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
  if (!for_float && !has_branch && !is_while && n == 3) {
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
  if (!for_float && !has_branch && !is_while && n == 5) {
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
        mul->a != add->a && !is_control_reg(add->a, for_a) &&
        !is_control_reg(mul->a, for_a)) {
      memset(seen, 0, sizeof(seen));
      d.nguards = 0;
      add_guard(&d, seen, add->a, add->ta);
      add_guard(&d, seen, for_a, LUAR_T_INT);
      add_guard(&d, seen, for_a + 1, LUAR_T_INT);
      add_guard(&d, seen, for_a + 2, LUAR_T_INT);
      d.fast_sq = 1;
      d.fs_float = (add->ta == LUAR_T_FLOAT);
      d.fs_acc = add->a;
      d.fs_temp = mul->a;
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
        /* LUAR: the fast loop keeps one accumulator.  A second write to
        ** it (for example `a=a+i*i; a=a*i`) must not be folded into the
        ** square/product peephole: the emitter chooses one operation and
        ** would compute a different sequence from the bytecode. */
        if (acc >= 0 || temp >= 0) { ok = 0; break; }
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
        if (acc >= 0 || temp >= 0 || op->b != idx ||
            op->ta != LUAR_T_FLOAT || op->a == idx) { ok = 0; break; }
        temp = op->a;
        is_float = 1;
        linear = 1;
        fconst = op->num;
      }
      else if (op->kind == LUAR_OP_DIV) {
        if (prod || acc >= 0 || temp >= 0 || op->a != op->b ||
            op->c != idx || op->ta != LUAR_T_FLOAT) { ok = 0; break; }
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
        if (prod || acc >= 0 || temp >= 0 || op->a != op->b ||
            op->c != idx ||
            (op->ta != LUAR_T_INT && op->ta != LUAR_T_FLOAT)) {
          ok = 0; break;
        }
        acc = op->a;
        is_float = (op->ta == LUAR_T_FLOAT);
        sub = 1;
        accum_i = (int)i;
      }
      else if (op->kind == LUAR_OP_ADDI) {
        /* The fast emitter keeps exactly one step for the induction
        ** variable.  A second increment (`i=i+1; i=i+1`, or
        ** `i=i+1; i=i+2`) used to overwrite d.fw_step and silently run a
        ** different loop than the interpreter.  Only one write is
        ** representable, so reject every additional one. */
        if (inc_i >= 0 || op->a != idx || op->b != idx || op->imm == 0 ||
            op->ta != LUAR_T_INT) { ok = 0; break; }
        inc_i = (int)i;
      }
      else ok = 0;
    }
    /* The fast path keeps the limit and the accumulator in registers for
    ** the whole run.  If the source reuses one register for both, or for
    ** the induction variable, the native loop would compare against the
    ** stale limit instead of the value the body writes. */
    if (ok && (acc == idx ||
               (limit >= 0 && (limit == acc || limit == idx)) ||
               (temp >= 0 && (temp == acc || temp == idx ||
                              (limit >= 0 && temp == limit)))))
      ok = 0;
    /* emit_fast_while compares the limit as a 64-bit integer.  A float
    ** limit would otherwise pass the later branch-guard pass (which
    ** re-reads the live type) and the native loop would compare against
    ** the double's raw bits, possibly running a different number of
    ** iterations or never terminating.  Let the generic branch emitter
    ** handle mixed int/float comparisons. */
    if (ok && limit >= 0 && ops[0].tb != LUAR_T_INT)
      ok = 0;
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
      d.fw_temp = (temp >= 0) ? temp : -1;
      d.fw_limit = limit >= 0 ? limit : 0xFF;
      d.fw_kind = kind;
      d.fw_k = ops[0].k;
      d.fw_imm = imm;
      d.fw_step = ops[inc_i].imm;
    }
  }

  /* Fast table sum: acc = acc + t[idx]. */
  if (!for_float && !is_while && d.nops >= 1 &&
      ops[0].kind == LUAR_OP_TABLE_ADD &&
      ops[0].c == for_a + 2) {
    int only = 1;
    for (i = 1; i < n - 1; i++)
      if (ops[i].kind != LUAR_OP_MMBIN && ops[i].kind != LUAR_OP_NOP)
        only = 0;
    if (only && d.ft_res != ops[0].a) {
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
      d.ft_sub = ops[0].is_sub;
      /* The native fast_tsum emitter only adds.  With tint disabled there
      ** is no C helper for subtraction, so leave the loop interpreted
      ** instead of silently turning it into an addition. */
      if (d.ft_sub && !J->enable_tint)
        return 0;
    }
  }

  /* Fast fill: t[idx] = idx with the loop index as both key and value. */
  if (!for_float && !is_while && d.nops == 1 &&
      ops[0].kind == LUAR_OP_SETTABLE &&
      ops[0].b == for_a + 2 && ops[0].c == for_a + 2) {
    d.fast_fill = 1;
    d.ff_table = ops[0].a;
  }
  /* Affine fill: t[i] = i + k, t[i] = i * k, t[i] = k.  The decoder has
  ** already rejected non-numeric values and control-register writes; the
  ** scalar is kept small so it can live in the existing tint field. */
  if (!for_float && !is_while && J->enable_tint && d.nops >= 1) {
    LuaR_Op *st = &ops[d.nops - 1];
    if (st->kind == LUAR_OP_SETTABLE && st->b == for_a + 2 &&
        st->a != for_a && st->a != for_a + 1 && st->a != for_a + 2) {
      if (d.nops == 1 && st->isconst && st->ta == LUAR_T_INT &&
          st->imm >= (int64_t)INT32_MIN && st->imm <= (int64_t)INT32_MAX)
        return install_affine_fill(p, J, backedge, for_a, st->a,
                                   LUAR_TINT_FILL_CONST, (int)st->imm);
      if (d.nops == 3 && !st->isconst && st->c == ops[0].a &&
          ops[0].b == for_a + 2 &&
          ops[1].kind == LUAR_OP_MMBIN &&
          ops[0].ta == LUAR_T_INT &&
          ops[0].imm >= (int64_t)INT32_MIN &&
          ops[0].imm <= (int64_t)INT32_MAX) {
        int mode;
        if (ops[0].kind == LUAR_OP_ADDI || ops[0].kind == LUAR_OP_ADDK)
          mode = LUAR_TINT_FILL_ADD;
        else if (ops[0].kind == LUAR_OP_MULK)
          mode = LUAR_TINT_FILL_MUL;
        else
          mode = 0;
        if (mode != 0)
          return install_affine_fill(p, J, backedge, for_a, st->a,
                                     mode, (int)ops[0].imm);
      }
    }
  }
  if (!for_float && (d.fast_fill || d.fast_tsum) && J->enable_tint) {
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
  p->luar_tint_suspend[slot] = 0;
    p->luar_tint_pc[slot] = (int)backedge;
    p->luar_tint_table[slot] = d.fast_fill ? d.ff_table : d.ft_table;
    p->luar_tint_acc[slot] = d.fast_tsum ? d.ft_acc : 0;
    p->luar_tint_res[slot] = d.fast_tsum ? d.ft_res : 0;
    p->luar_tint_arg4[slot] = d.fast_tsum ? d.ft_sub : 0;
    p->luar_tint_base[slot] = for_a;
    p->luar_tint_epoch[slot] = J->epoch;
    J->compiled++;
    return 1;
  }
  if (d.has_table && !J->enable_table)
    return 0;

  /* Fast integer sum: acc = acc + idx. */
  if (!for_float && !has_branch && !is_while && n == 3) {
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
  if (!for_float && !has_branch && !is_while && n >= 3) {
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
      d.ff_temp = temp;
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
        case LUAR_OP_MOD:
        case LUAR_OP_IDIV:
          reads[nr] = op->b; types[nr++] = op->tb;
          reads[nr] = op->c; types[nr++] = op->tc;
          break;
        case LUAR_OP_ADDI:
          reads[nr] = op->b; types[nr++] = LUAR_T_INT; break;
        case LUAR_OP_GETTABLE:
          reads[nr] = op->b; types[nr++] = LUAR_T_TABLE;
          reads[nr] = op->c; types[nr++] = LUAR_T_INT;
          break;
        case LUAR_OP_GETI:
          reads[nr] = op->b; types[nr++] = LUAR_T_TABLE;
          break;
        case LUAR_OP_TABLE_ADDI:
          reads[nr] = op->b; types[nr++] = LUAR_T_TABLE;
          reads[nr] = op->a; types[nr++] = op->ta;
          break;
        case LUAR_OP_TABLE_ADD:
          reads[nr] = op->b; types[nr++] = LUAR_T_TABLE;
          reads[nr] = op->c; types[nr++] = LUAR_T_INT;
          reads[nr] = op->a; types[nr++] = op->ta;
          break;
        case LUAR_OP_SETTABLE:
          reads[nr] = op->a; types[nr++] = LUAR_T_TABLE;
          reads[nr] = op->b; types[nr++] = LUAR_T_INT;
          if (!op->isconst) {
            reads[nr] = op->c; types[nr++] = op->ta;
          }
          break;
        case LUAR_OP_SETI:
          reads[nr] = op->a; types[nr++] = LUAR_T_TABLE;
          if (!op->isconst) {
            reads[nr] = op->c; types[nr++] = op->ta;
          }
          break;
        case LUAR_OP_BAND:
        case LUAR_OP_BOR:
        case LUAR_OP_BXOR:
          reads[nr] = op->b; types[nr++] = LUAR_T_INT;
          reads[nr] = op->c; types[nr++] = LUAR_T_INT;
          break;
        /* LUAR: K arithmetic and unary minus may be float-typed.  The
        ** entry guard must match the operand flavour the decoder proved;
        ** using LUAR_T_INT made every float branch loop compile but fail
        ** its own entry guard, so it silently stayed interpreted. */
        case LUAR_OP_ADDK:
        case LUAR_OP_SUBK:
        case LUAR_OP_MULK:
        case LUAR_OP_DIVK:
        case LUAR_OP_UNM:
          reads[nr] = op->b; types[nr++] = op->tb; break;
        case LUAR_OP_MODK:
        case LUAR_OP_IDIVK:
        case LUAR_OP_BANDK:
        case LUAR_OP_BORK:
        case LUAR_OP_BXORK:
        case LUAR_OP_BNOT:
        case LUAR_OP_SHRI:
        case LUAR_OP_SHLI:
          reads[nr] = op->b; types[nr++] = LUAR_T_INT; break;
        case LUAR_OP_SHL:
        case LUAR_OP_SHR:
          reads[nr] = op->b; types[nr++] = LUAR_T_INT;
          reads[nr] = op->c; types[nr++] = LUAR_T_INT;
          break;
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
        case LUAR_OP_MOD:
        case LUAR_OP_IDIV:
        case LUAR_OP_ADDK:
        case LUAR_OP_SUBK:
        case LUAR_OP_MULK:
        case LUAR_OP_DIVK:
        case LUAR_OP_MODK:
        case LUAR_OP_IDIVK:
        case LUAR_OP_BAND:
        case LUAR_OP_BOR:
        case LUAR_OP_BXOR:
        case LUAR_OP_BANDK:
        case LUAR_OP_BORK:
        case LUAR_OP_BXORK:
        case LUAR_OP_BNOT:
        case LUAR_OP_UNM:
        case LUAR_OP_SHRI:
        case LUAR_OP_SHLI:
        case LUAR_OP_SHL:
        case LUAR_OP_SHR:
        case LUAR_OP_GETTABLE:
        case LUAR_OP_TABLE_ADD:
        case LUAR_OP_GETI:
        case LUAR_OP_TABLE_ADDI:
          last_write[op->a] = (int)i;
          break;
        default: break;
      }
      if ((op->kind == LUAR_OP_TABLE_ADD ||
           op->kind == LUAR_OP_TABLE_ADDI) && op->res != 0xFF)
        last_write[op->res] = (int)i;
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
      int ct = for_float ? LUAR_T_FLOAT : LUAR_T_INT;
      for (i = 0; i < 3; i++)
        add_guard(&d, seen, for_a + i, ct);
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
    LuaR_Op *addi = NULL, *accop = NULL, *tempop = NULL;
    int idx = -1, acc = -1, limit = -1, imm = 0, left, opkind = 0;
    int addi_i = -1, acc_i = -1, temp = -1, temp_i = -1;
    int body_ok = 1;
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
        tempop = op;
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
    /* The fast repeat emitter runs only the increment and the accumulator
    ** operation.  Any other instruction in the body would be dropped, so
    ** require the body to consist solely of the matched operations and
    ** MMBIN/NOP placeholders.  The comparison itself is implemented by the
    ** emitter's termination test. */
    for (i = 0; i < d.nops; i++) {
      LuaR_Op *op = &ops[i];
      if (op == cmp || op == addi || op == accop ||
          (opkind == 3 && op == tempop))
        continue;
      if (op->kind == LUAR_OP_MMBIN || op->kind == LUAR_OP_NOP)
        continue;
      body_ok = 0;
      break;
    }
    if (body_ok && addi != NULL && accop != NULL && acc_i > addi_i &&
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
      /* LUAR: a float constant comparison (`until i == 3.5`) is recorded
      ** as OP_EQK with the value in num, not imm; the fast repeat emitter
      ** compares against the integer immediate and would terminate at zero.
      ** Such a loop has no integer solution anyway, so leave it to the
      ** interpreter instead of emitting a compare against the wrong value. */
      if (imm && cmp->b == 0xFF && cmp->tb != LUAR_T_INT)
        acc = -1;
      /* Same aliasing rule as the fast while path: the cached limit must
      ** be a different register from the accumulator and the counter. */
      if (acc == idx || (limit >= 0 && (limit == acc || limit == idx)) ||
          (temp >= 0 && (temp == acc || temp == idx ||
                         (limit >= 0 && temp == limit))))
        acc = -1;
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
        d.fr_temp = (opkind == 3 && temp >= 0) ? temp : -1;
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
  /* Any loop that reuses a register with different types is unsafe in a
  ** loop-level emitter that guards tags only at entry: the native loop does
  ** not re-check the tag of a loop-carried slot on each iteration.  This is
  ** not limited to branch bodies; a straight-line body such as
  ** `t = u + i * 1.5; u = s - i` changes `u` from an integer to a float and
  ** the next iteration reads the double payload as an integer.  The
  ** specialised single-pattern paths and partial prefixes have their own
  ** register contracts / entry checks. */
  if (!d.partial && !d.fast_while && !d.fast_repeat &&
      !d.fast_meta && !d.fast_closure && !loop_types_stable(&d))
    return 0;
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
    p->luar_fld_type[slot] = (uint8_t)d.ffld_type;
    p->luar_fld_table[slot] = d.ffld_table;
    p->luar_fld_key[slot] = d.ffld_key;
    p->luar_fld_imm[slot] = d.ffld_imm;
    p->luar_fld_num[slot] = d.ffld_num;
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
    luaH_resizearray(L, h, (unsigned)last);   /* keep existing hash keys */
  for (k = 0; k <= count; k++) {
    TValue v;
    setivalue(&v, idx + k);
    obj2arr(h, (unsigned)(idx - 1 + k), &v);
  }
  setivalue(ct, 0);
  setivalue(it, idx + count);
  return 1;
}


/* Mode 1: key + scalar, mode 2: key * scalar, mode 3: scalar.  The loop
** must still be an ascending unit-step run; anything else stays on the
** interpreter.  Integer arithmetic wraps exactly like the VM's. */
static int for_run_last_any (lua_Integer idx, lua_Integer count,
                             lua_Integer step, lua_Integer *last);


static int table_affine_fill_intrinsic (lua_State *L, TValue *base,
                                        int for_a, int table_reg,
                                        int mode, int scalar) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *tt = &base[table_reg];
  lua_Integer count, step, idx, last, key, k;
  int64_t sv = (int64_t)scalar;
  Table *h;
  if (!ttistable(tt) || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last_any(idx, count, step, &last) ||
      count > (1 << 22))
    return 0;
  if ((idx > last ? idx : last) > (1 << 22))
    return 0;
  h = hvalue(tt);
  if (h->metatable != NULL)
    return 0;
  if (last > 0 && h->asize < (unsigned)last)
    luaH_resizearray(L, h, (unsigned)last);   /* keep existing hash keys */
  for (k = 0, key = idx; k <= count; k++, key += step) {
    TValue v;
    if (mode == LUAR_TINT_FILL_CONST) {
      setivalue(&v, sv);
    }
    else if (mode == LUAR_TINT_FILL_MUL) {
      setivalue(&v, l_castU2S(l_castS2U(key) * l_castS2U(sv)));
    }
    else if (mode == LUAR_TINT_FILL_SUB) {
      setivalue(&v, l_castU2S(l_castS2U(key) - l_castS2U(sv)));
    }
    else {
      setivalue(&v, l_castU2S(l_castS2U(key) + l_castS2U(sv)));
    }
    if ((lua_Unsigned)key - 1 < h->asize)
      obj2arr(h, (unsigned)(key - 1), &v);
    else
      luaH_setint(L, h, key, &v);
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}





/* A numeric for-loop runs 'count + 1' further iterations starting at Lua
** index 'idx' (the index after the interpreter's FORLOOP), then advances by
** 'step'.  Return the last Lua index, rejecting values that would overflow.
** Both positive and negative steps are valid. */
static int for_run_last (lua_Integer idx, lua_Integer count,
                         lua_Integer step, lua_Integer *last) {
  lua_Integer delta;
  if (count < 0 || step == 0 || idx <= 0)
    return 0;
  if (step > 0) {
    if (count > LUA_MAXINTEGER / step)
      return 0;
  }
  else {
    if (step == LUA_MININTEGER ||
        count > LUA_MAXINTEGER / (-step))
      return 0;
  }
  delta = count * step;
  if ((step > 0 && idx > LUA_MAXINTEGER - delta) ||
      (step < 0 && idx < LUA_MININTEGER - delta))
    return 0;
  *last = idx + delta;
  return 1;
}


/* Same FORLOOP endpoint arithmetic, but without the positive-key
** restriction.  Generic table copies/fills can address hash keys at any
** integer index. */
static int for_run_last_any (lua_Integer idx, lua_Integer count,
                             lua_Integer step, lua_Integer *last) {
  lua_Integer delta;
  if (count < 0 || step == 0)
    return 0;
  if (step > 0) {
    if (count > LUA_MAXINTEGER / step)
      return 0;
  }
  else {
    if (step == LUA_MININTEGER ||
        count > LUA_MAXINTEGER / (-step))
      return 0;
  }
  delta = count * step;
  if ((step > 0 && idx > LUA_MAXINTEGER - delta) ||
      (step < 0 && idx < LUA_MININTEGER - delta))
    return 0;
  *last = idx + delta;
  return 1;
}


/* All keys of a run are between idx and last; a monotonic integer step
** cannot leave the array part if both endpoints are inside it. */
static int array_run_bounds (Table *h, lua_Integer idx, lua_Integer last) {
  if (h == NULL || h->metatable != NULL)
    return 0;
  if (idx < 1 || last < 1)
    return 0;
  if ((lua_Unsigned)idx > (lua_Unsigned)h->asize ||
      (lua_Unsigned)last > (lua_Unsigned)h->asize)
    return 0;
  return 1;
}


/* Check that every element the run will read is numeric.  The caller has
** already validated the run's bounds, so ckey stays in [0, asize). */
static int array_run_numeric (Table *h, lua_Integer idx, lua_Integer count,
                              lua_Integer step) {
  lua_Integer ckey = idx - 1;
  lua_Integer k;
  for (k = 0; k <= count; k++) {
    lu_byte tag = *getArrTag(h, (unsigned)ckey);
    if (tag != LUA_VNUMINT && tag != LUA_VNUMFLT)
      return 0;
    ckey += step;
  }
  return 1;
}


/* Raw array sum over an already resolved table.  Used by the register and
** upvalue-table intrinsic entries. */
static int table_sum_table (lua_State *L, TValue *base, int for_a, Table *h,
                            int acc_reg, int res_reg, int sub) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_reg];
  lua_Integer count, step, idx, last, ckey, k;
  (void)L;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last(idx, count, step, &last) ||
      !array_run_bounds(h, idx, last))
    return 0;
  if (ttisinteger(at)) {
    lua_Unsigned sum = l_castS2U(ivalue(at));
    lua_Number fsum = 0;
    int promoted = 0;
    for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
      unsigned key = (unsigned)ckey;
      lu_byte tag = *getArrTag(h, key);
      if (tag == LUA_VNUMINT) {
        if (promoted) {
          if (sub) fsum -= cast_num(getArrVal(h, key)->i);
          else fsum += cast_num(getArrVal(h, key)->i);
        }
        else if (sub)
          sum -= l_castS2U(getArrVal(h, key)->i);
        else
          sum += l_castS2U(getArrVal(h, key)->i);
      }
      else if (tag == LUA_VNUMFLT) {
        /* LUAR: an integer accumulator becomes a float as soon as a float
        ** element is added, exactly as the interpreter's mixed arithmetic
        ** does.  Keep the already accumulated integer exact while there is
        ** no float, then promote once. */
        if (!promoted) {
          fsum = cast_num(l_castU2S(sum));
          promoted = 1;
        }
        if (sub) fsum -= getArrVal(h, key)->n;
        else fsum += getArrVal(h, key)->n;
      }
      else
        return 0;
    }
    if (promoted) {
      setfltvalue(at, fsum);
    }
    else {
      chgivalue(at, l_castU2S(sum));
    }
  }
  else if (ttisfloat(at)) {
    lua_Number sum = fltvalue(at);
    for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
      unsigned key = (unsigned)ckey;
      lu_byte tag = *getArrTag(h, key);
      if (tag == LUA_VNUMINT) {
        if (sub) sum -= cast_num(getArrVal(h, key)->i);
        else sum += cast_num(getArrVal(h, key)->i);
      }
      else if (tag == LUA_VNUMFLT) {
        if (sub) sum -= getArrVal(h, key)->n;
        else sum += getArrVal(h, key)->n;
      }
      else
        return 0;
    }
    setfltvalue(at, sum);
  }
  else
    return 0;
  if (res_reg != acc_reg && res_reg >= 0) {
    unsigned rkey = (unsigned)(last - 1);
    lu_byte rtag = *getArrTag(h, rkey);
    if (rtag == LUA_VNUMINT) {
      setivalue(&base[res_reg], getArrVal(h, rkey)->i);
    }
    else {
      setfltvalue(&base[res_reg], getArrVal(h, rkey)->n);
    }
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int table_sum_intrinsic (lua_State *L, TValue *base,
                                int for_a, int table_reg, int acc_reg,
                                int res_reg, int sub) {
  TValue *tt = &base[table_reg];
  if (!ttistable(tt))
    return 0;
  return table_sum_table(L, base, for_a, hvalue(tt), acc_reg, res_reg, sub);
}


static int keyed_fill_intrinsic (lua_State *L, TValue *base,
                                int for_a, int table_reg, Table *keys,
                                int key_temp) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *tt = &base[table_reg];
  lua_Integer count, step, idx, last, k;
  Table *t;
  TValue last_key;
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
  setnilvalue(&last_key);
  for (k = 0; k <= count; k++) {
    TValue key, val;
    if (luaH_getint(keys, idx + k, &key) == LUA_VNIL)
      return 0;
    last_key = key;
    setivalue(&val, idx + k);
    luaH_set(L, t, &key, &val);
  }
  if (key_temp >= 0 && key_temp < 256)
    base[key_temp] = last_key;
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int keyed_sum_intrinsic (lua_State *L, TValue *base,
                               int for_a, int table_reg, Table *keys,
                               int acc_reg, int res_reg) {
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
  if (count < 0 || step != 1 || idx <= 0)
    return 0;
  if (idx > LUA_MAXINTEGER - count)
    return 0;
  last = idx + count;
  t = hvalue(tt);
  if (t->metatable != NULL || keys->metatable != NULL)
    return 0;
  if (ttisinteger(at)) {
    lua_Unsigned sum = l_castS2U(ivalue(at));
    lua_Number fsum = 0;
    int promoted = 0;
    for (k = 0; k <= count; k++) {
      TValue key, res;
      lu_byte tag;
      if (luaH_getint(keys, idx + k, &key) == LUA_VNIL)
        return 0;
      tag = luaH_get(t, &key, &res);
      if (tag == LUA_VNUMINT) {
        if (promoted)
          fsum += cast_num(ivalue(&res));
        else
          sum += l_castS2U(ivalue(&res));
      }
      else if (tag == LUA_VNUMFLT) {
        /* LUAR: promote an integer accumulator on the first float value. */
        if (!promoted) {
          fsum = cast_num(l_castU2S(sum));
          promoted = 1;
        }
        fsum += fltvalue(&res);
      }
      else
        return 0;
    }
    if (promoted) {
      setfltvalue(at, fsum);
    }
    else {
      chgivalue(at, l_castU2S(sum));
    }
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
  if (res_reg != acc_reg && res_reg >= 0) {
    TValue key, val;
    if (luaH_getint(keys, idx + count, &key) != LUA_VNIL &&
        luaH_get(t, &key, &val) != LUA_VNIL)
      base[res_reg] = val;
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* A number in the two flavours Lua arithmetic distinguishes.  The pair
** intrinsics use this to reproduce the interpreter's int-wrap / float-
** promotion rules exactly, including a promotion in the middle of a run. */
typedef struct {
  int isflt;
  int64_t i;
  double d;
} LuaR_PairNum;


static LuaR_PairNum pair_int (int64_t v) {
  LuaR_PairNum n;
  n.isflt = 0;
  n.i = v;
  n.d = 0.0;
  return n;
}


static LuaR_PairNum pair_flt (double v) {
  LuaR_PairNum n;
  n.isflt = 1;
  n.i = 0;
  n.d = v;
  return n;
}


static double pair_number (LuaR_PairNum n) {
  return n.isflt ? n.d : cast_num(n.i);
}


static LuaR_PairNum pair_add (LuaR_PairNum a, LuaR_PairNum b) {
  if (a.isflt || b.isflt)
    return pair_flt(pair_number(a) + pair_number(b));
  return pair_int(l_castU2S(l_castS2U(a.i) + l_castS2U(b.i)));
}


static LuaR_PairNum pair_sub (LuaR_PairNum a, LuaR_PairNum b) {
  if (a.isflt || b.isflt)
    return pair_flt(pair_number(a) - pair_number(b));
  return pair_int(l_castU2S(l_castS2U(a.i) - l_castS2U(b.i)));
}


static LuaR_PairNum pair_mul (LuaR_PairNum a, LuaR_PairNum b) {
  if (a.isflt || b.isflt)
    return pair_flt(pair_number(a) * pair_number(b));
  return pair_int(l_castU2S(l_castS2U(a.i) * l_castS2U(b.i)));
}


/* Lua's '/' always produces a float, even for two integers.  Division by
** zero follows IEEE (inf/NaN), never a Lua error. */
static LuaR_PairNum pair_div (LuaR_PairNum a, LuaR_PairNum b) {
  return pair_flt(pair_number(a) / pair_number(b));
}


/* Lua integer floor division/modulo with wrap-defined INT64_MIN / -1.
** A zero divisor is never passed here: the table-store helper declines
** before writing so the interpreter can raise the exact error. */
static int64_t pair_idiv_i (int64_t a, int64_t b) {
  int64_t q;
  if (b == -1)
    return l_castU2S(0u - l_castS2U(a));
  q = a / b;
  if ((a ^ b) < 0 && a % b != 0)
    q--;
  return q;
}


static int64_t pair_mod_i (int64_t a, int64_t b) {
  if (b == -1)
    return 0;
  {
    int64_t r = a % b;
    if (r != 0 && ((r ^ b) < 0))
      r += b;
    return r;
  }
}


static LuaR_PairNum pair_idiv (LuaR_PairNum a, LuaR_PairNum b) {
  if (a.isflt || b.isflt)
    return pair_flt(floor(pair_number(a) / pair_number(b)));
  return pair_int(pair_idiv_i(a.i, b.i));
}


static LuaR_PairNum pair_mod (LuaR_PairNum a, LuaR_PairNum b) {
  if (a.isflt || b.isflt) {
    double x = pair_number(a), y = pair_number(b);
    double m = fmod(x, y);
    if ((m > 0) ? y < 0 : (m < 0 && y > 0))
      m += y;
    return pair_flt(m);
  }
  return pair_int(pair_mod_i(a.i, b.i));
}


static LuaR_PairNum pair_neg (LuaR_PairNum a) {
  if (a.isflt)
    return pair_flt(-a.d);
  return pair_int(l_castU2S(0u - l_castS2U(a.i)));
}


static LuaR_PairNum pair_bnot (LuaR_PairNum a) {
  return pair_int(~a.i);
}


static LuaR_PairNum pair_band (LuaR_PairNum a, LuaR_PairNum b) {
  return pair_int(a.i & b.i);
}

static LuaR_PairNum pair_bor (LuaR_PairNum a, LuaR_PairNum b) {
  return pair_int(a.i | b.i);
}

static LuaR_PairNum pair_bxor (LuaR_PairNum a, LuaR_PairNum b) {
  return pair_int(a.i ^ b.i);
}

static LuaR_PairNum pair_shift (LuaR_PairNum a, LuaR_PairNum b) {
  int64_t n = b.i;
  if (n < 0) {
    if (n <= -64)
      return pair_int(0);
    return pair_int(l_castU2S(l_castS2U(a.i) >> (unsigned)(-n)));
  }
  if (n >= 64)
    return pair_int(0);
  return pair_int(l_castU2S(l_castS2U(a.i) << (unsigned)n));
}


static LuaR_PairNum pair_from_slot (const TValue *v, int *ok) {
  if (ttisinteger(v))
    return pair_int(ivalue(v));
  if (ttisfloat(v))
    return pair_flt(fltvalue(v));
  *ok = 0;
  return pair_int(0);
}


static LuaR_PairNum pair_from_array (Table *h, unsigned key) {
  if (*getArrTag(h, key) == LUA_VNUMFLT)
    return pair_flt(getArrVal(h, key)->n);
  return pair_int(getArrVal(h, key)->i);
}


static Table *pair_source_table (TValue *base, int reg, int upval);


static void pair_store_slot (TValue *base, int reg, LuaR_PairNum v,
                             int acc_reg);


static int table_sum3_intrinsic (lua_State *L, TValue *base, int for_a,
                               int s1, int s2, int s3, int acc_reg,
                               int res_reg, int mid_reg, int elem2_reg) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_reg];
  Table *h[3];
  int enc[3];
  LuaR_PairNum acc, last_mid, last_b, last_c;
  lua_Integer count, step, idx, last, ckey, k;
  int j, ok = 1;
  (void)L;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last(idx, count, step, &last))
    return 0;
  enc[0] = s1;
  enc[1] = s2;
  enc[2] = s3;
  for (j = 0; j < 3; j++) {
    int reg = (enc[j] >= 0) ? enc[j] : -1;
    int up = (enc[j] >= 0) ? -1 : (-1 - enc[j]);
    h[j] = pair_source_table(base, reg, up);
    if (!array_run_bounds(h[j], idx, last) ||
        !array_run_numeric(h[j], idx, count, step))
      return 0;
  }
  acc = pair_from_slot(at, &ok);
  if (!ok)
    return 0;
  last_mid = acc;
  last_b = pair_int(0);
  last_c = pair_int(0);
  for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
    unsigned key = (unsigned)ckey;
    LuaR_PairNum a = pair_from_array(h[0], key);
    LuaR_PairNum b = pair_from_array(h[1], key);
    LuaR_PairNum c = pair_from_array(h[2], key);
    last_b = b;
    last_c = c;
    /* Bytecode: mid = acc + a; mid = mid + b; acc = mid + c. */
    last_mid = pair_add(pair_add(acc, a), b);
    acc = pair_add(last_mid, c);
  }
  /* Preserve the visible temporaries.  Write in bytecode order so aliases
  ** end with the last value the interpreter would store: the second
  ** element, then the intermediate, then the third element. */
  pair_store_slot(base, elem2_reg, last_b, acc_reg);
  pair_store_slot(base, mid_reg, last_mid, acc_reg);
  pair_store_slot(base, res_reg, last_c, acc_reg);
  pair_store_slot(base, acc_reg, acc, -1);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static Table *pair_source_table (TValue *base, int reg, int upval) {
  TValue *tv = pair_source_value(base, reg, upval);
  return (tv != NULL && ttistable(tv)) ? hvalue(tv) : NULL;
}


/* kind 12: s = s + a[i] + b[i]
** kind 13: s = s + a[i] - b[i]
** kind 14: s = s + a[i] * b[i]  (a and b may be the same table)
** kind 36: s = s + a[i] / b[i]  (division does not commute)
** kind 38: s = s - a[i] * b[i]
** kind 40: s = s - a[i] / b[i]
** kind 43/44: s = s + a[i] // b[i] / a[i] % b[i]
** kind 45/46: the subtract spellings */
static void pair_store_slot (TValue *base, int reg, LuaR_PairNum v,
                             int acc_reg) {
  if (reg < 0 || reg == acc_reg)
    return;
  if (v.isflt)
    { setfltvalue(&base[reg], v.d); }
  else
    { setivalue(&base[reg], v.i); }
}


static int pair_array_intrinsic (lua_State *L, TValue *base, int for_a,
                                 int areg, int aup, int breg, int bup,
                                 int acc_reg, int res_reg, int res2_reg,
                                 int res3_reg, int kind) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_reg];
  Table *ha, *hb;
  LuaR_PairNum acc, last_temp, last_a, last_b;
  lua_Integer count, step, idx, last, ckey, k;
  int ok = 1;
  (void)L;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  ha = pair_source_table(base, areg, aup);
  hb = pair_source_table(base, breg, bup);
  if (!for_run_last(idx, count, step, &last) ||
      !array_run_bounds(ha, idx, last) ||
      !array_run_numeric(ha, idx, count, step))
    return 0;
  if (hb != ha &&
      (!array_run_bounds(hb, idx, last) ||
       !array_run_numeric(hb, idx, count, step)))
    return 0;
  if (!ttisinteger(at) && !ttisfloat(at))
    return 0;
  acc = pair_from_slot(at, &ok);
  if (!ok)
    return 0;
  last_temp = acc;
  last_a = pair_int(0);
  last_b = pair_int(0);
  for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
    unsigned key = (unsigned)ckey;
    LuaR_PairNum av = pair_from_array(ha, key);
    LuaR_PairNum bv = pair_from_array(hb, key);
    last_a = av;
    last_b = bv;
    if (kind == 14 || kind == 36 || kind == 38 || kind == 40 ||
        kind == 43 || kind == 44 || kind == 45 || kind == 46) {
      int div = (kind == 36 || kind == 40);
      int idiv = (kind == 43 || kind == 45);
      int mod = (kind == 44 || kind == 46);
      int sub = (kind == 38 || kind == 40 || kind == 45 || kind == 46);
      LuaR_PairNum prod;
      if ((idiv || mod) && !av.isflt && !bv.isflt && bv.i == 0)
        return 0;   /* integer zero divisor: the interpreter raises */
      if (idiv)
        prod = pair_idiv(av, bv);
      else if (mod)
        prod = pair_mod(av, bv);
      else if (div)
        prod = pair_div(av, bv);
      else
        prod = pair_mul(av, bv);
      last_temp = prod;
      acc = sub ? pair_sub(acc, prod) : pair_add(acc, prod);
    }
    else {
      LuaR_PairNum t = pair_add(acc, av);
      last_temp = t;
      acc = (kind == 13) ? pair_sub(t, bv) : pair_add(t, bv);
    }
  }
  if (acc.isflt)
    { setfltvalue(at, acc.d); }
  else
    { setivalue(at, acc.i); }
  pair_store_slot(base, res_reg, last_temp, acc_reg);
  /* The first element register can be the destination of the fused
  ** arithmetic (the compiler reuses a dead temp).  Its final bytecode
  ** value is then the intermediate, not the element. */
  pair_store_slot(base, res2_reg,
                  (res2_reg == res_reg) ? last_temp : last_a, acc_reg);
  pair_store_slot(base, res3_reg,
                  (res3_reg == res_reg) ? last_temp : last_b, acc_reg);
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* Resolve a scaled-sum scalar encoding to its current value.  Constants
** come from Proto->k (integer or float, never collectable), register
** scalars from the frame and upvalue scalars from the current closure. */
static TValue *tint_scalar (Proto *p, TValue *base, int enc) {
  if (enc >= LUAR_TINT_SCALAR_CONST) {
    unsigned k = (unsigned)(enc - LUAR_TINT_SCALAR_CONST);
    if (k >= (unsigned)p->sizek)
      return NULL;
    return &p->k[k];
  }
  if (enc >= 0) {
    if (enc > 255)
      return NULL;
    return &base[enc];
  }
  {
    int up = -1 - enc;
    TValue *fv = &base[-1];
    LClosure *cl;
    if (!ttisLclosure(fv))
      return NULL;
    cl = clLvalue(fv);
    if (up < 0 || up >= cl->p->sizeupvalues)
      return NULL;
    return cl->upvals[up]->v.p;
  }
}


/* Copy a numeric source array into a destination table (a closure
** upvalue for kind 57).  The source and destination may be the same. */
static int table_copy_to_intrinsic (lua_State *L, TValue *base, int for_a,
                                    Table *dst, TValue *srcv, int tmp_reg) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  Table *src;
  LuaR_PairNum last;
  lua_Integer count, step, idx, lastkey, maxkey, ckey, k;
  int fast;
  if (dst == NULL || srcv == NULL || !ttistable(srcv))
    return 0;
  src = hvalue(srcv);
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last_any(idx, count, step, &lastkey) ||
      count > (1 << 22))
    return 0;
  fast = array_run_bounds(src, idx, lastkey) &&
         array_run_numeric(src, idx, count, step);
  maxkey = (idx > lastkey) ? idx : lastkey;
  if (dst->metatable != NULL || maxkey > (1 << 22))
    return 0;
  if (maxkey > 0 && dst->asize < (unsigned)maxkey)
    luaH_resizearray(L, dst, (unsigned)maxkey);
  last = pair_int(0);
  if (fast) {
    for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
      LuaR_PairNum v = pair_from_array(src, (unsigned)ckey);
      TValue tv;
      last = v;
      if (v.isflt) {
        setfltvalue(&tv, v.d);
      }
      else {
        setivalue(&tv, v.i);
      }
      obj2arr(dst, (unsigned)ckey, &tv);
    }
    pair_store_slot(base, tmp_reg, last, -1);
  }
  else {
    TValue lastv;
    setnilvalue(&lastv);
    for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
      TValue v;
      setnilvalue(&v);
      luaH_getint(src, (lua_Integer)ckey + 1, &v);
      luaH_setint(L, dst, (lua_Integer)ckey + 1, &v);
      lastv = v;
    }
    base[tmp_reg] = lastv;
  }
  setivalue(ct, 0);
  setivalue(it, lastkey);
  return 1;
}


/* Fill a table range with the loop key itself. */
/* Affine fill with a runtime integer scalar (register/upvalue). */
static int table_affine_scalar_fill_intrinsic (lua_State *L, TValue *base,
                                               int for_a, Table *dst,
                                               TValue *scalar, int mode,
                                               int tmp_reg) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  lua_Integer count, step, idx, last, key, k;
  int64_t sv;
  double sd;
  int sflt;
  if (dst == NULL || scalar == NULL ||
      (!ttisinteger(scalar) && !ttisfloat(scalar)) ||
      !ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last_any(idx, count, step, &last) ||
      count > (1 << 22))
    return 0;
  if ((idx > last ? idx : last) > (1 << 22))
    return 0;
  sflt = ttisfloat(scalar);
  sv = sflt ? 0 : (int64_t)ivalue(scalar);
  sd = sflt ? fltvalue(scalar) : 0.0;
  if (dst->metatable != NULL)
    return 0;
  if (last > 0 && dst->asize < (unsigned)last)
    luaH_resizearray(L, dst, (unsigned)last);
  for (k = 0, key = idx; k <= count; k++, key += step) {
    TValue v;
    if (sflt) {
      double dk = (double)key;
      if (mode == LUAR_TINT_FILL_MUL) {
        setfltvalue(&v, dk * sd);
      }
      else if (mode == LUAR_TINT_FILL_SUB) {
        setfltvalue(&v, dk - sd);
      }
      else {
        setfltvalue(&v, dk + sd);
      }
    }
    else if (mode == LUAR_TINT_FILL_MUL) {
      setivalue(&v, l_castU2S(l_castS2U(key) * l_castS2U(sv)));
    }
    else if (mode == LUAR_TINT_FILL_SUB) {
      setivalue(&v, l_castU2S(l_castS2U(key) - l_castS2U(sv)));
    }
    else {
      setivalue(&v, l_castU2S(l_castS2U(key) + l_castS2U(sv)));
    }
    if ((lua_Unsigned)key - 1 < dst->asize)
      obj2arr(dst, (unsigned)(key - 1), &v);
    else
      luaH_setint(L, dst, key, &v);
    if (tmp_reg >= 0)
      base[tmp_reg] = v;
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


static int table_index_fill_intrinsic (lua_State *L, TValue *base, int for_a,
                                       Table *dst) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  lua_Integer count, step, idx, lastkey, maxkey, ckey, k;
  if (dst == NULL || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last_any(idx, count, step, &lastkey) ||
      count > (1 << 22))
    return 0;
  maxkey = (idx > lastkey) ? idx : lastkey;
  if (dst->metatable != NULL || maxkey > (1 << 22))
    return 0;
  if (maxkey > 0 && dst->asize < (unsigned)maxkey)
    luaH_resizearray(L, dst, (unsigned)maxkey);
  for (k = 0, ckey = idx; k <= count; k++, ckey += step) {
    TValue v;
    setivalue(&v, ckey);
    luaH_setint(L, dst, ckey, &v);
  }
  setivalue(ct, 0);
  setivalue(it, lastkey);
  return 1;
}


/* Fill a run with one arbitrary TValue.  luaH_setint handles GC barriers
** and removes an entry for a nil value. */
static int table_fill_any_intrinsic (lua_State *L, TValue *base, int for_a,
                                     Table *dst, TValue *val, int tmp_reg) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  lua_Integer count, step, idx, lastkey, maxkey, ckey, k;
  if (dst == NULL || val == NULL || !ttisinteger(ct) ||
      !ttisinteger(st) || !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last_any(idx, count, step, &lastkey) ||
      count > (1 << 22))
    return 0;
  maxkey = (idx > lastkey) ? idx : lastkey;
  if (dst->metatable != NULL || maxkey > (1 << 22))
    return 0;
  if (maxkey > 0 && dst->asize < (unsigned)maxkey)
    luaH_resizearray(L, dst, (unsigned)maxkey);
  for (k = 0, ckey = idx; k <= count; k++, ckey += step)
    luaH_setint(L, dst, ckey, val);
  if (tmp_reg >= 0)
    base[tmp_reg] = *val;
  setivalue(ct, 0);
  setivalue(it, lastkey);
  return 1;
}


/* Clear a run of keys in an existing table.  Array slots are cleared
** directly; sparse keys use luaH_setint (which removes hash entries). */
static int table_nil_clear (lua_State *L, TValue *base, int for_a,
                            Table *h) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  lua_Integer count, step, idx, last, ckey, k;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last_any(idx, count, step, &last) ||
      count > (1 << 22))
    return 0;
  if (h == NULL || h->metatable != NULL ||
      (idx > last ? idx : last) > (1 << 22))
    return 0;
  for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
    unsigned key = (unsigned)ckey;
    if ((lua_Unsigned)ckey < h->asize) {
      *getArrTag(h, key) = LUA_VEMPTY;
      memset(getArrVal(h, key), 0, sizeof(Value));
    }
    else {
      TValue nilv;
      setnilvalue(&nilv);
      luaH_setint(L, h, (lua_Integer)ckey + 1, &nilv);
    }
  }
  setivalue(ct, 0);
  setivalue(it, last);
  return 1;
}


/* kind 51: dst[i] = nil over a raw table array. */
static int table_nil_store_intrinsic (lua_State *L, TValue *base, int for_a,
                                      int dst_reg) {
  TValue *tt = &base[dst_reg];
  if (!ttistable(tt))
    return 0;
  return table_nil_clear(L, base, for_a, hvalue(tt));
}


/* kind 54: dst[i] = a[i] OP b[i] over two raw table arrays. */
static int table_pair_store_intrinsic (lua_State *L, TValue *base,
                                       int for_a, int dst_reg,
                                       int aenc, int benc, int op,
                                       int elem1_reg, int elem2_reg,
                                       int res_reg) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *tt = &base[dst_reg];
  Table *ta, *tb, *dst;
  LuaR_PairNum last_a = pair_int(0), last_b = pair_int(0), last_res = pair_int(0);
  lua_Integer count, step, idx, lastkey, maxkey, ckey, k;
  int areg = (aenc >= 0) ? aenc : -1;
  int aup = (aenc >= 0) ? -1 : (-1 - aenc);
  int breg = (benc >= 0) ? benc : -1;
  int bup = (benc >= 0) ? -1 : (-1 - benc);
  if (!ttistable(tt) || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last(idx, count, step, &lastkey))
    return 0;
  ta = pair_source_table(base, areg, aup);
  tb = pair_source_table(base, breg, bup);
  if (!array_run_bounds(ta, idx, lastkey) ||
      !array_run_numeric(ta, idx, count, step) ||
      !array_run_bounds(tb, idx, lastkey) ||
      !array_run_numeric(tb, idx, count, step))
    return 0;
  if ((op == 5 || op == 6)) {
    /* Integer/integer zero divisor raises in the interpreter.  A float
    ** operand takes the non-raising path. */
    for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
      lu_byte xa = *getArrTag(ta, (unsigned)ckey);
      lu_byte xb = *getArrTag(tb, (unsigned)ckey);
      if (xa == LUA_VNUMINT && xb == LUA_VNUMINT &&
          getArrVal(tb, (unsigned)ckey)->i == 0)
        return 0;
    }
  }
  if (op >= 7) {
    for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
      if (*getArrTag(ta, (unsigned)ckey) != LUA_VNUMINT ||
          *getArrTag(tb, (unsigned)ckey) != LUA_VNUMINT)
        return 0;   /* bitwise operands must be integers */
    }
  }
  dst = hvalue(tt);
  maxkey = (idx > lastkey) ? idx : lastkey;
  if (dst->metatable != NULL || maxkey > (1 << 22))
    return 0;
  if (dst->asize < (unsigned)maxkey)
    luaH_resizearray(L, dst, (unsigned)maxkey);
  for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
    unsigned key = (unsigned)ckey;
    LuaR_PairNum x = pair_from_array(ta, key);
    LuaR_PairNum y = pair_from_array(tb, key);
    TValue v;
    last_a = x;
    last_b = y;
    if (op == 0)
      last_res = pair_add(x, y);
    else if (op == 1)
      last_res = pair_sub(x, y);
    else if (op == 2)
      last_res = pair_mul(x, y);
    else if (op == 3)
      last_res = pair_div(x, y);
    else if (op == 5)
      last_res = pair_idiv(x, y);
    else if (op == 6)
      last_res = pair_mod(x, y);
    else if (op == 7)
      last_res = pair_band(x, y);
    else if (op == 8)
      last_res = pair_bor(x, y);
    else if (op == 9)
      last_res = pair_bxor(x, y);
    else if (op == 10)
      last_res = pair_shift(x, y);
    else
      last_res = pair_shift(x, pair_neg(y));
    if (last_res.isflt) {
      setfltvalue(&v, last_res.d);
    }
    else {
      setivalue(&v, last_res.i);
    }
    obj2arr(dst, key, &v);
  }
  pair_store_slot(base, elem1_reg, last_a, -1);
  pair_store_slot(base, elem2_reg, last_b, -1);
  pair_store_slot(base, res_reg, last_res, -1);
  setivalue(ct, 0);
  setivalue(it, lastkey);
  return 1;
}


/* kind 42: dst[i] = src[i] +|-|*|/ k over raw table arrays.  The run is
** checked against the source array before any store; the destination may
** be the same table and is grown only when the source run is known to be
** safe.  Metatables are rejected so __index/__newindex never matter. */
static int table_scaled_store_intrinsic (lua_State *L, Proto *p,
                                         TValue *base, int for_a,
                                         int dst_reg, int src_enc,
                                         int scalar_enc, int scalar_imm,
                                         int elem_reg, int res_reg,
                                         int op) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *tt = &base[dst_reg];
  Table *src, *dst;
  TValue *sc;
  TValue immval;
  LuaR_PairNum fac, last_elem, last_res;
  lua_Integer count, step, idx, lastkey, maxkey, ckey, k;
  int sreg = (src_enc >= 0) ? src_enc : -1;
  int sup = (src_enc >= 0) ? -1 : (-1 - src_enc);
  int ok = 1;
  if (!ttistable(tt) || !ttisinteger(ct) || !ttisinteger(st) ||
      !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last(idx, count, step, &lastkey))
    return 0;
  src = pair_source_table(base, sreg, sup);
  if (!array_run_bounds(src, idx, lastkey) ||
      !array_run_numeric(src, idx, count, step))
    return 0;
  if (op == 4 || op == 7 || op == 8) {
    fac = pair_int(0);   /* unused copy/unary marker */
  }
  else {
    if (scalar_enc == LUAR_TINT_SCALAR_IMM) {
      setivalue(&immval, (lua_Integer)scalar_imm);
      sc = &immval;
    }
    else
      sc = tint_scalar(p, base, scalar_enc);
    if (sc == NULL || (!ttisinteger(sc) && !ttisfloat(sc)))
      return 0;
    fac = pair_from_slot(sc, &ok);
    if (!ok)
      return 0;
    /* An integer zero divisor raises for an integer element, but a float
    ** element takes the non-raising float path.  The interpreter visits
    ** elements in order, so decline the whole run if an integer could be
    ** reached; if every element is float, the helper is safe. */
    if ((op == 5 || op == 6) && ttisinteger(sc) && ivalue(sc) == 0) {
      for (ckey = idx - 1, k = 0; k <= count; k++, ckey += step) {
        if (*getArrTag(src, (unsigned)ckey) != LUA_VNUMFLT)
          return 0;
      }
    }
  }
  if (op == 8 || op >= 9) {
    for (ckey = idx - 1, k = 0; k <= count; k++, ckey += step) {
      if (*getArrTag(src, (unsigned)ckey) != LUA_VNUMINT)
        return 0;   /* float bitwise conversion stays interpreted */
    }
    if (op >= 9 && fac.isflt)
      return 0;   /* scalar must be an integer */
  }
  dst = hvalue(tt);
  maxkey = (idx > lastkey) ? idx : lastkey;
  if (dst->metatable != NULL || maxkey > (1 << 22))
    return 0;
  if (dst->asize < (unsigned)maxkey)
    luaH_resizearray(L, dst, (unsigned)maxkey);
  last_elem = pair_int(0);
  last_res = pair_int(0);
  for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
    LuaR_PairNum elem = pair_from_array(src, (unsigned)ckey);
    TValue v;
    last_elem = elem;
    if (op == 0)
      last_res = pair_add(elem, fac);
    else if (op == 1)
      last_res = pair_sub(elem, fac);
    else if (op == 2)
      last_res = pair_mul(elem, fac);
    else if (op == 3)
      last_res = pair_div(elem, fac);
    else if (op == 5)
      last_res = pair_idiv(elem, fac);
    else if (op == 6)
      last_res = pair_mod(elem, fac);
    else if (op == 7)
      last_res = pair_neg(elem);
    else if (op == 8)
      last_res = pair_bnot(elem);
    else if (op == 9)
      last_res = pair_band(elem, fac);
    else if (op == 10)
      last_res = pair_bor(elem, fac);
    else if (op == 11)
      last_res = pair_bxor(elem, fac);
    else if (op == 12)
      last_res = pair_shift(elem, fac);
    else if (op == 13)
      last_res = pair_shift(elem, pair_neg(fac));
    else
      last_res = elem;   /* copy */

    if (last_res.isflt) {
      setfltvalue(&v, last_res.d);
    }
    else {
      setivalue(&v, last_res.i);
    }
    obj2arr(dst, (unsigned)ckey, &v);
  }
  pair_store_slot(base, elem_reg, last_elem, -1);
  pair_store_slot(base, res_reg, last_res, -1);
  setivalue(ct, 0);
  setivalue(it, lastkey);
  return 1;
}


/* kind 15: s = s + a[i] * k.  The scalar is numeric in every iteration;
** the element and product registers keep their last bytecode values. */
static int scaled_array_intrinsic (lua_State *L, TValue *base, int for_a,
                                   Table *h, const TValue *scalar,
                                   int elem_reg, int prod_reg, int acc_reg,
                                   int div, int sub) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_reg];
  lua_Integer count, step, idx, last, ckey, k;
  LuaR_PairNum acc, fac, last_elem, last_prod;
  int ok = 1;
  (void)L;
  if (!ttisinteger(ct) || !ttisinteger(st) || !ttisinteger(it))
    return 0;
  count = ivalue(ct);
  step = ivalue(st);
  idx = ivalue(it);
  if (!for_run_last(idx, count, step, &last) ||
      !array_run_bounds(h, idx, last) ||
      !array_run_numeric(h, idx, count, step))
    return 0;
  if (!ttisinteger(scalar) && !ttisfloat(scalar))
    return 0;
  if (!ttisinteger(at) && !ttisfloat(at))
    return 0;
  acc = pair_from_slot(at, &ok);
  fac = pair_from_slot(scalar, &ok);
  if (!ok)
    return 0;
  last_elem = pair_int(0);
  last_prod = pair_int(0);
  for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
    unsigned key = (unsigned)ckey;
    LuaR_PairNum elem = pair_from_array(h, key);
    last_elem = elem;
    last_prod = div ? pair_div(elem, fac) : pair_mul(elem, fac);
    acc = sub ? pair_sub(acc, last_prod) : pair_add(acc, last_prod);
  }
  if (acc.isflt)
    { setfltvalue(at, acc.d); }
  else
    { setivalue(at, acc.i); }
  if (elem_reg >= 0 && elem_reg != prod_reg)
    pair_store_slot(base, elem_reg, last_elem, acc_reg);
  pair_store_slot(base, prod_reg, last_prod, acc_reg);
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
      GETARG_B(cl->p->code[0]) != 0 ||
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
  /* The method must read and write the same field on self.  Using only
  ** the GETFIELD key made `self.w = self.v + x` accumulate into self.v
  ** and leave self.w untouched. */
  if (GETARG_C(cl->p->code[0]) != GETARG_B(cl->p->code[3]) ||
      GETARG_C(cl->p->code[0]) >= cl->p->sizek)
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
        /* LUAR: a zero divisor must raise in the interpreter, not become
        ** a zero result.  Bail out of the intrinsic and let T0 report the
        ** exact 'attempt to divide by zero' / n//0 error. */
        if ((op == OP_MOD || op == OP_IDIV) && r[c] == 0)
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
        if ((op == OP_MODK || op == OP_IDIVK) && ivalue(k) == 0)
          return 0;   /* LUAR: let the interpreter raise the zero-divisor error */
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


/* LUAR: resolve the table captured in an upvalue for the upvalue-sum
** intrinsic.  Returns NULL when the closure/upvalue is not a table. */
static Table *tint_upval_table (Proto *p, TValue *base, int slot) {
  TValue *fv = &base[-1];
  LClosure *cl;
  int idx = p->luar_tint_upval[slot];
  TValue *v;
  if (!ttisLclosure(fv))
    return NULL;
  cl = clLvalue(fv);
  if (idx < 0 || idx >= cl->p->sizeupvalues)
    return NULL;
  v = cl->upvals[idx]->v.p;
  if (!ttistable(v))
    return NULL;
  return hvalue(v);
}


static int ir_floatify (Proto *p, TValue *base, LuaR_TraceIR *ir);
static int ir_integer_only (const LuaR_TraceIR *ir);
static int ir_side_allows (Proto *p, TValue *base, LuaR_TraceIR *ir);

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


LUAR_NO_SANITIZE_FUNCTION
static int luari_run_j4 (Proto *p, void *code, void *base, lua_State *L) {
  LuaR_JIT *J = G(L)->luar_jit;
  ptrdiff_t topoff = (char *)L->top.p - (char *)L->stack.p;
  StackValue *want = (StackValue *)base + p->maxstacksize;
  int r;
  if (want > L->top.p && want <= L->stack_last.p)
    L->top.p = want;   /* root every frame slot while native helpers may GC */
  if (J != NULL) J->active_depth++;
  r = ((LuaR_LoopFn)code)(base, L);
  if (J != NULL) {
    J->active_depth--;
    if (J->active_depth == 0 && J->flush_pending) {
      J->flush_pending = 0;
      free_code(J);
    }
  }
  L->top.p = (StkId)((char *)L->stack.p + topoff);
  return r;
}


LUAR_NO_SANITIZE_FUNCTION
static int jit_ir_side (LuaR_JIT *J, Proto *p, int pc, void *base, int expc, lua_State *L) {
  int sr, lastpc = expc;
  if (p->luar_side_code == NULL && !p->luar_ir_side_done) {
    LuaR_TraceIR ir;
    int link;
    p->luar_ir_side_done = 1;
    if (expc < 0 || expc > pc ||
        !luar_trace_record_loop(p, expc, pc, (TValue *)base, L, &ir))
      return -1;
    if (!ir_side_allows(p, (TValue *)base, &ir))
      return -1;   /* mixed shape: do not emit integer side code */
    link = luar_trace_ir_link(&ir, 0, p->luar_ir_start);
    (void)luar_trace_ir_analyze(&ir, NULL, 0);
    if (link == 0 || (uint32_t)ir.depth > LUAR_IR_MAX_DEPTH) {
      J->ir_side_aborts++;
      return -1;
    }
    if (link == 2) J->ir_cycles++;
    J->ir_side_start = expc;
    J->ir_side_parent = p->luar_ir_start;
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
  sr = luari_run_j4(p, p->luar_side_code, base, L);
  base = (void *)(L->ci->func.p + 1);
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
        if (!luar_trace_record_loop(p, startpc, pc, (TValue *)base, L, &cir))
          break;
        if (!ir_side_allows(p, (TValue *)base, &cir))
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
        J->ir_side_parent = p->luar_ir_start;
        J->ir_side_depth = cir.depth;
        if ((uint32_t)cir.depth > J->ir_max_depth)
          J->ir_max_depth = (uint32_t)cir.depth;
      }
      if (p->luar_child_code[level] == NULL ||
          p->luar_child_epoch[level] != J->epoch ||
          p->luar_child_pc[level] != pc ||
          p->luar_child_start[level] != startpc)
        break;
      csr = luari_run_j4(p, p->luar_child_code[level], base, L);
      base = (void *)(L->ci->func.p + 1);
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
  if (lastpc >= 0)
    return -(lastpc + 2);
  return -1;
}


LUAR_NO_SANITIZE_FUNCTION
int luari_jit_continue (lua_State *L, Proto *p, int pc, void *base) {
  LuaR_JIT *J = G(L)->luar_jit;
  int r = 0;
  if (L == NULL || p == NULL || base == NULL || J == NULL || !J->enabled)
    return 0;
  if (L->hookmask != 0 || !J->enable_ir || p->luar_cont_code == NULL ||
      p->luar_cont_epoch != J->epoch ||
      p->luar_cont_call_pc < 0 || pc != p->luar_cont_call_pc + 1)
    return 0;
  r = luari_run_j4(p, p->luar_cont_code, base, L);
  if (r > 0) {
    J->ir_cont_runs++;
    return p->luar_cont_backedge + 1;
  }
  if (r < -1)
    return r;
  return 0;
}


LUAR_NO_SANITIZE_FUNCTION
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
    J->ir_entry_checks++;
    if (p->luar_ir_pc != pc)
      J->ir_entry_pc_mismatch++;
    if (p->luar_ir_base != expected_base)
      J->ir_entry_base_mismatch++;
    if (p->luar_ir_epoch != J->epoch) {
      p->luar_ir_code = NULL;
      p->luar_ir_size = 0;
      p->luar_ir_pc = -1;
      p->luar_ir_start = -1;
      p->luar_ir_base = -1;
      p->luar_ir_partial = 0;
      p->luar_ir_call_pc = -1;
    }
    else if (p->luar_ir_pc == pc && p->luar_ir_base == expected_base) {
      int cb = p->luar_ir_base;
      /* J4 roots only integer-controlled FORLOOPs.  A float-controlled
      ** loop can reach the same pc with the same base, so re-check the
      ** control triplet at entry instead of trusting the record-time
      ** values. */
      if (cb >= 0) {
        TValue *bv = (TValue *)base;
        if (cb + 2 >= p->maxstacksize ||
            !ttisinteger(&bv[cb]) || !ttisinteger(&bv[cb + 1]) ||
            !ttisinteger(&bv[cb + 2])) {
          J->ir_entry_failures++;
          return 0;
        }
      }
      int irr = luari_run_j4(p, p->luar_ir_code, base, L);
      base = (void *)(L->ci->func.p + 1);  /* stack may have moved */
      J->ir_last_entry_result = irr;
      if (irr <= 0)
        J->ir_entry_failures++;
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
        p->luar_tint_base[i] == expected_base &&
        p->luar_tint_suspend[i] != L->ci->luar_gen) {
      int r = 0;
      int kind = p->luar_tint[i];
      if (p->luar_tint_epoch[i] != J->epoch) {
        p->luar_tint[i] = 0;
        continue;
      }
      switch (kind) {
        case 1:
          r = table_fill_intrinsic(L, (TValue *)base, expected_base,
                                   p->luar_tint_table[i]);
          break;
        case 2:
          r = table_sum_intrinsic(L, (TValue *)base, expected_base,
                                  p->luar_tint_table[i],
                                  p->luar_tint_acc[i],
                                  p->luar_tint_res[i],
                                  p->luar_tint_arg4[i]);
          break;
        case 3: {
          Table *keys = tint_keys_table(p, (TValue *)base, i);
          r = (keys == NULL) ? 0
              : keyed_fill_intrinsic(L, (TValue *)base, expected_base,
                                     p->luar_tint_table[i], keys,
                                     p->luar_tint_arg4[i]);
          break;
        }
        case 4: {
          Table *keys = tint_keys_table(p, (TValue *)base, i);
          r = (keys == NULL) ? 0
              : keyed_sum_intrinsic(L, (TValue *)base, expected_base,
                                    p->luar_tint_table[i], keys,
                                    p->luar_tint_acc[i],
                                    p->luar_tint_res[i]);
          break;
        }
        case 42:
          r = table_scaled_store_intrinsic(
                  L, p, (TValue *)base, expected_base,
                  p->luar_tint_table[i], p->luar_tint_arg4[i],
                  p->luar_tint_keys[i], p->luar_tint_res[i],
                  p->luar_tint_upval[i] & 0xff,
                  (p->luar_tint_upval[i] >> 8) & 0xff,
                  p->luar_tint_acc[i]);
          break;
        case 67: {
          int dstreg = p->luar_tint_table[i];
          int venc = p->luar_tint_keys[i];
          TValue *vv = pair_source_value((TValue *)base, -1, -1 - venc);
          r = (vv == NULL) ? 0
              : luar_ffi_trace_fill_array_i64(
                    &((TValue *)base)[dstreg],
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    vv);
          break;
        }
        case 94:
          r = luar_ffi_trace_copy_array(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[p->luar_tint_keys[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]]);
          break;
        case 93:
          r = luar_ffi_trace_copy_array(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[p->luar_tint_keys[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]]);
          break;
        case 92: {
          int kidx = p->luar_tint_arg4[i];
          double v;
          if (kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          if (ttisfloat(&p->k[kidx]))
            v = fltvalue(&p->k[kidx]);
          else if (ttisinteger(&p->k[kidx]))
            v = (double)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_array_f64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], v, L);
          break;
        }
        case 91: {
          int aenc = p->luar_tint_table[i];
          int venc = p->luar_tint_keys[i];
          TValue *av = (aenc >= 0) ? &((TValue *)base)[aenc]
                       : pair_source_value((TValue *)base, -1, -1 - aenc);
          TValue *vv = (venc >= 0) ? &((TValue *)base)[venc]
                       : pair_source_value((TValue *)base, -1, -1 - venc);
          double v;
          if (av == NULL || vv == NULL) {
            r = 0;
            break;
          }
          if (ttisfloat(vv))
            v = fltvalue(vv);
          else if (ttisinteger(vv))
            v = (double)ivalue(vv);
          else if (!luar_ffi_trace_f64_value(vv, &v)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_array_f64(
                  av, &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], v, L);
          break;
        }
        case 95: {
          int enc = p->luar_tint_res[i];
          TValue *sv = (enc >= 0) ? &((TValue *)base)[enc]
                       : pair_source_value((TValue *)base, -1, -1 - enc);
          double sb;
          if (sv == NULL || (!ttisfloat(sv) && !ttisinteger(sv))) {
            r = 0;
            break;
          }
          sb = ttisfloat(sv) ? fltvalue(sv) : (double)ivalue(sv);
          r = luar_ffi_trace_update_array_f64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], sb,
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 89: {
          int kidx = p->luar_tint_arg4[i];
          double v;
          if (kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          if (ttisfloat(&p->k[kidx]))
            v = fltvalue(&p->k[kidx]);
          else if (ttisinteger(&p->k[kidx]))
            v = (double)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_array_f64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], v, L);
          break;
        }
        case 88: {
          int enc = p->luar_tint_keys[i];
          TValue *vv = (enc >= 0) ? &((TValue *)base)[enc]
                       : pair_source_value((TValue *)base, -1, -1 - enc);
          double v;
          if (vv == NULL) {
            r = 0;
            break;
          }
          if (ttisfloat(vv))
            v = fltvalue(vv);
          else if (ttisinteger(vv))
            v = (double)ivalue(vv);
          else if (!luar_ffi_trace_f64_value(vv, &v)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_array_f64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], v, L);
          break;
        }
        case 86: {
          int kidx = p->luar_tint_arg4[i];
          double sb;
          if (kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          if (ttisfloat(&p->k[kidx]))
            sb = fltvalue(&p->k[kidx]);
          else if (ttisinteger(&p->k[kidx]))
            sb = (double)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_array_f64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], sb,
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 85: {
          int enc = p->luar_tint_table[i];
          int venc = p->luar_tint_keys[i];
          TValue *cv = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          TValue *vv = pair_source_value((TValue *)base,
                                         (venc >= 0) ? venc : -1,
                                         (venc >= 0) ? -1 : (-1 - venc));
          uint64_t bits;
          if (cv == NULL || vv == NULL) {
            r = 0;
            break;
          }
          if (ttisboolean(vv))
            bits = l_isfalse(vv) ? 0 : 1;
          else if (ttisinteger(vv))
            bits = (uint64_t)ivalue(vv);
          else if (ttisfloat(vv)) {
            double d = fltvalue(vv);
            if (!isfinite(d) || d < -9.0e18 || d > 9.0e18) {
              r = 0;
              break;
            }
            bits = (uint64_t)(int64_t)d;
          }
          else if (luar_ffi_trace_int_value(vv, &bits)) {
            /* integer cdata scalar */
          }
          else {
            double d;
            if (!luar_ffi_trace_f64_value(vv, &d) ||
                !isfinite(d) || d < -9.0e18 || d > 9.0e18) {
              r = 0;
              break;
            }
            bits = (uint64_t)(int64_t)d;
          }
          r = luar_ffi_trace_fill_field_i64(
                  cv, (size_t)p->luar_tint_acc[i], bits,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], L);
          break;
        }
        case 103: {
          TValue *fv = &((TValue *)base)[-1];
          LClosure *cl;
          int up = p->luar_tint_table[i];
          int kidx = p->luar_tint_arg4[i];
          TValue *uv;
          double sb;
          if (!ttisLclosure(fv)) {
            r = 0;
            break;
          }
          cl = clLvalue(fv);
          if (up < 0 || up >= cl->p->sizeupvalues ||
              cl->upvals[up] == NULL || kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          uv = cl->upvals[up]->v.p;
          if (ttisfloat(&p->k[kidx]))
            sb = fltvalue(&p->k[kidx]);
          else if (ttisinteger(&p->k[kidx]))
            sb = (double)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_field_f64(
                  uv, (size_t)p->luar_tint_acc[i], sb,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 102: {
          TValue *fv = &((TValue *)base)[-1];
          LClosure *cl;
          int up = p->luar_tint_table[i];
          int enc = p->luar_tint_res[i];
          TValue *uv, *sv;
          double sb;
          if (!ttisLclosure(fv)) {
            r = 0;
            break;
          }
          cl = clLvalue(fv);
          if (up < 0 || up >= cl->p->sizeupvalues ||
              cl->upvals[up] == NULL) {
            r = 0;
            break;
          }
          uv = cl->upvals[up]->v.p;
          sv = (enc >= 0) ? &((TValue *)base)[enc]
               : pair_source_value((TValue *)base, -1, -1 - enc);
          if (sv == NULL) {
            r = 0;
            break;
          }
          if (ttisfloat(sv))
            sb = fltvalue(sv);
          else if (ttisinteger(sv))
            sb = (double)ivalue(sv);
          else if (!luar_ffi_trace_f64_value(sv, &sb)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_field_f64(
                  uv, (size_t)p->luar_tint_acc[i], sb,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 101: {
          int enc = p->luar_tint_res[i];
          TValue *sv = (enc >= 0) ? &((TValue *)base)[enc]
                       : pair_source_value((TValue *)base, -1, -1 - enc);
          double sb;
          if (sv == NULL) {
            r = 0;
            break;
          }
          if (ttisfloat(sv))
            sb = fltvalue(sv);
          else if (ttisinteger(sv))
            sb = (double)ivalue(sv);
          else if (!luar_ffi_trace_f64_value(sv, &sb)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_field_f64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  (size_t)p->luar_tint_acc[i], sb,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 100: {
          int kidx = p->luar_tint_arg4[i];
          double sb;
          if (kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          if (ttisfloat(&p->k[kidx]))
            sb = fltvalue(&p->k[kidx]);
          else if (ttisinteger(&p->k[kidx]))
            sb = (double)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_field_f64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  (size_t)p->luar_tint_acc[i], sb,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 99: {
          int enc = p->luar_tint_table[i];
          int venc = p->luar_tint_keys[i];
          TValue *cv = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          TValue *vv = (venc >= 0) ? &((TValue *)base)[venc]
                       : pair_source_value((TValue *)base, -1, -1 - venc);
          double dv;
          if (cv == NULL || vv == NULL) {
            r = 0;
            break;
          }
          if (ttisfloat(vv))
            dv = fltvalue(vv);
          else if (ttisinteger(vv))
            dv = (double)ivalue(vv);
          else if (!luar_ffi_trace_f64_value(vv, &dv)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_field_f64(
                  cv, (size_t)p->luar_tint_acc[i], dv,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], L);
          break;
        }
        case 107: {
          int enc = p->luar_tint_res[i];
          TValue *sv = (enc >= 0) ? &((TValue *)base)[enc]
                       : pair_source_value((TValue *)base, -1, -1 - enc);
          double sb;
          if (sv == NULL) {
            r = 0;
            break;
          }
          if (ttisfloat(sv))
            sb = fltvalue(sv);
          else if (ttisinteger(sv))
            sb = (double)ivalue(sv);
          else if (!luar_ffi_trace_f64_value(sv, &sb)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_field_f32(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  (size_t)p->luar_tint_acc[i], sb,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 106: {
          int kidx = p->luar_tint_arg4[i];
          double sb;
          if (kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          if (ttisfloat(&p->k[kidx]))
            sb = fltvalue(&p->k[kidx]);
          else if (ttisinteger(&p->k[kidx]))
            sb = (double)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_field_f32(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  (size_t)p->luar_tint_acc[i], sb,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 105: {
          int enc = p->luar_tint_table[i];
          int venc = p->luar_tint_keys[i];
          TValue *cv = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          TValue *vv = (venc >= 0) ? &((TValue *)base)[venc]
                       : pair_source_value((TValue *)base, -1, -1 - venc);
          double dv;
          if (cv == NULL || vv == NULL) {
            r = 0;
            break;
          }
          if (ttisfloat(vv))
            dv = fltvalue(vv);
          else if (ttisinteger(vv))
            dv = (double)ivalue(vv);
          else if (!luar_ffi_trace_f64_value(vv, &dv)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_field_f32(
                  cv, (size_t)p->luar_tint_acc[i], dv,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], L);
          break;
        }
        case 104: {
          int enc = p->luar_tint_table[i];
          int kidx = p->luar_tint_arg4[i];
          TValue *cv = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          double dv;
          if (cv == NULL || kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          if (ttisfloat(&p->k[kidx]))
            dv = fltvalue(&p->k[kidx]);
          else if (ttisinteger(&p->k[kidx]))
            dv = (double)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_field_f32(
                  cv, (size_t)p->luar_tint_acc[i], dv,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], L);
          break;
        }
        case 98: {
          int enc = p->luar_tint_table[i];
          int kidx = p->luar_tint_arg4[i];
          TValue *cv = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          double dv;
          if (cv == NULL || kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          if (ttisfloat(&p->k[kidx]))
            dv = fltvalue(&p->k[kidx]);
          else if (ttisinteger(&p->k[kidx]))
            dv = (double)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_field_f64(
                  cv, (size_t)p->luar_tint_acc[i], dv,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], L);
          break;
        }
        case 84: {
          int enc = p->luar_tint_table[i];
          int kidx = p->luar_tint_arg4[i];
          TValue *cv = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          uint64_t bits;
          if (cv == NULL || kidx < 0 || kidx >= p->sizek) {
            r = 0;
            break;
          }
          if (ttisboolean(&p->k[kidx]))
            bits = l_isfalse(&p->k[kidx]) ? 0 : 1;
          else if (ttisinteger(&p->k[kidx]))
            bits = (uint64_t)(int64_t)ivalue(&p->k[kidx]);
          else {
            r = 0;
            break;
          }
          r = luar_ffi_trace_fill_field_i64(
                  cv, (size_t)p->luar_tint_acc[i], bits,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], L);
          break;
        }
        case 83: {
          int denc = p->luar_tint_table[i];
          int senc = p->luar_tint_keys[i];
          TValue *dv = pair_source_value((TValue *)base,
                                         (denc >= 0) ? denc : -1,
                                         (denc >= 0) ? -1 : (-1 - denc));
          TValue *sv = pair_source_value((TValue *)base,
                                         (senc >= 0) ? senc : -1,
                                         (senc >= 0) ? -1 : (-1 - senc));
          r = (dv == NULL || sv == NULL) ? 0
              : luar_ffi_trace_copy_field_i64(
                    dv, sv, (size_t)p->luar_tint_acc[i],
                    (size_t)p->luar_tint_res[i],
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &((TValue *)base)[p->luar_tint_upval[i]],
                    p->luar_tint_upval[i], L);
          break;
        }
        case 108:
          r = luar_ffi_trace_copy_field_float(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[p->luar_tint_keys[i]],
                  (size_t)p->luar_tint_acc[i],
                  (size_t)p->luar_tint_res[i],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], L);
          break;
        case 82:
          r = luar_ffi_trace_copy_field_i64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[p->luar_tint_keys[i]],
                  (size_t)p->luar_tint_acc[i],
                  (size_t)p->luar_tint_res[i],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], L);
          break;
        case 81: {
          TValue *fv = &((TValue *)base)[-1];
          LClosure *cl;
          int up = p->luar_tint_table[i];
          TValue *uv;
          if (!ttisLclosure(fv)) {
            r = 0;
            break;
          }
          cl = clLvalue(fv);
          if (up < 0 || up >= cl->p->sizeupvalues ||
              cl->upvals[up] == NULL) {
            r = 0;
            break;
          }
          uv = cl->upvals[up]->v.p;
          r = luar_ffi_trace_sum_field_i64(
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  uv, (size_t)p->luar_tint_acc[i],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_res[i]],
                  p->luar_tint_res[i], p->luar_tint_keys[i], L);
          break;
        }
        case 80:
          r = luar_ffi_trace_sum_field_i64(
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  &((TValue *)base)[p->luar_tint_table[i]],
                  (size_t)p->luar_tint_acc[i],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_res[i]],
                  p->luar_tint_res[i], p->luar_tint_keys[i], L);
          break;
        case 79: {
          TValue *fv = &((TValue *)base)[-1];
          LClosure *cl;
          int up = p->luar_tint_table[i];
          int enc = p->luar_tint_res[i];
          TValue *uv, *sv;
          uint64_t bits;
          if (!ttisLclosure(fv)) {
            r = 0;
            break;
          }
          cl = clLvalue(fv);
          if (up < 0 || up >= cl->p->sizeupvalues ||
              cl->upvals[up] == NULL) {
            r = 0;
            break;
          }
          uv = cl->upvals[up]->v.p;
          sv = (enc >= 0) ? &((TValue *)base)[enc]
               : pair_source_value((TValue *)base, -1, -1 - enc);
          if (sv == NULL || !luar_ffi_trace_int_value(sv, &bits)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_field_i64(
                  uv, (size_t)p->luar_tint_acc[i], bits,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 78: {
          TValue *fv = &((TValue *)base)[-1];
          LClosure *cl;
          int up = p->luar_tint_table[i];
          TValue *uv;
          if (!ttisLclosure(fv)) {
            r = 0;
            break;
          }
          cl = clLvalue(fv);
          if (up < 0 || up >= cl->p->sizeupvalues ||
              cl->upvals[up] == NULL) {
            r = 0;
            break;
          }
          uv = cl->upvals[up]->v.p;
          r = luar_ffi_trace_update_field_i64(
                  uv, (size_t)p->luar_tint_acc[i],
                  (uint64_t)(int64_t)p->luar_tint_arg4[i],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 77: {
          int enc = p->luar_tint_res[i];
          TValue *sv = (enc >= 0) ? &((TValue *)base)[enc]
                       : pair_source_value((TValue *)base, -1, -1 - enc);
          uint64_t bits;
          if (sv == NULL) {
            r = 0;
            break;
          }
          if (ttisinteger(sv))
            bits = (uint64_t)ivalue(sv);
          else if (!luar_ffi_trace_int_value(sv, &bits)) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_update_field_i64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  (size_t)p->luar_tint_acc[i], bits,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        }
        case 76:
          r = luar_ffi_trace_update_field_i64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  (size_t)p->luar_tint_acc[i],
                  (uint64_t)(int64_t)p->luar_tint_arg4[i],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_keys[i], L);
          break;
        case 75: {
          int arr = p->luar_tint_table[i];
          int enc = p->luar_tint_keys[i];
          TValue *sv = (enc >= 0) ? &((TValue *)base)[enc]
                       : pair_source_value((TValue *)base, -1, -1 - enc);
          r = (sv == NULL) ? 0
              : luar_ffi_trace_update_array_i64(
                    &((TValue *)base)[arr],
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    sv, &((TValue *)base)[p->luar_tint_upval[i]],
                    p->luar_tint_upval[i], p->luar_tint_acc[i], L);
          break;
        }
        case 74: {
          int arr = p->luar_tint_table[i];
          int enc = p->luar_tint_keys[i];
          TValue *sv = pair_source_value((TValue *)base, -1, -1 - enc);
          r = (sv == NULL) ? 0
              : luar_ffi_trace_update_array_i64(
                    &((TValue *)base)[arr],
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    sv, &((TValue *)base)[p->luar_tint_upval[i]],
                    p->luar_tint_upval[i], p->luar_tint_acc[i], L);
          break;
        }
        case 73:
          r = luar_ffi_trace_update_array_i64_bits(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  (uint64_t)(int64_t)p->luar_tint_arg4[i],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_acc[i], L);
          break;
        case 71:
          r = luar_ffi_trace_update_array_i64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_keys[i]],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  p->luar_tint_upval[i], p->luar_tint_acc[i], L);
          break;
        case 69: {
          int enc = p->luar_tint_table[i];
          TValue *av = pair_source_value((TValue *)base, -1, -1 - enc);
          r = (av == NULL) ? 0
              : luar_ffi_trace_sum_array_i64(
                    &((TValue *)base)[p->luar_tint_acc[i]], av,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &((TValue *)base)[p->luar_tint_res[i]],
                    p->luar_tint_res[i], p->luar_tint_arg4[i], L);
          break;
        }
        case 68:
          r = luar_ffi_trace_sum_array_i64(
                  &((TValue *)base)[p->luar_tint_acc[i]],
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_res[i]],
                  p->luar_tint_res[i], p->luar_tint_arg4[i], L);
          break;
        case 66:
          r = luar_ffi_trace_fill_array_i64(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_keys[i]]);
          break;
        case 65: {
          int dstreg = p->luar_tint_table[i];
          int srcenc = p->luar_tint_keys[i];
          int tmpreg = p->luar_tint_upval[i];
          TValue *sv = pair_source_value((TValue *)base,
                                         (srcenc >= 0) ? srcenc : -1,
                                         (srcenc >= 0) ? -1 : (-1 - srcenc));
          r = (sv == NULL) ? 0
              : luar_ffi_trace_copy_array_i64(
                    &((TValue *)base)[dstreg], sv,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &((TValue *)base)[tmpreg], tmpreg, L);
          break;
        }
        case 64: {
          int dstreg = p->luar_tint_table[i];
          int srcreg = p->luar_tint_keys[i];
          int tmpreg = p->luar_tint_upval[i];
          r = luar_ffi_trace_copy_array_i64(
                  &((TValue *)base)[dstreg], &((TValue *)base)[srcreg],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[tmpreg], tmpreg, L);
          break;
        }
        case 63: {
          TValue *sc = tint_scalar(p, (TValue *)base,
                                    p->luar_tint_upval[i]);
          if (sc == NULL || (!ttisinteger(sc) && !ttisfloat(sc)) ||
              !ttistable(&((TValue *)base)[p->luar_tint_table[i]])) {
            r = 0;
            break;
          }
          r = table_affine_scalar_fill_intrinsic(
                  L, (TValue *)base, expected_base,
                  hvalue(&((TValue *)base)[p->luar_tint_table[i]]),
                  sc, p->luar_tint_keys[i], p->luar_tint_arg4[i]);
          break;
        }
        case 62:
          if (!ttistable(&((TValue *)base)[p->luar_tint_table[i]])) {
            r = 0;
            break;
          }
          r = table_index_fill_intrinsic(
                  L, (TValue *)base, expected_base,
                  hvalue(&((TValue *)base)[p->luar_tint_table[i]]));
          break;
        case 61: {
          int denc = p->luar_tint_table[i];
          int enc = p->luar_tint_keys[i];
          int arg4 = p->luar_tint_arg4[i];
          TValue *val;
          if (!ttistable(&((TValue *)base)[denc])) {
            r = 0;
            break;
          }
          if (enc == -1)
            val = &p->k[arg4];
          else if (enc >= 0)
            val = &((TValue *)base)[enc];
          else {
            TValue *fv = &((TValue *)base)[-1];
            LClosure *cl;
            if (!ttisLclosure(fv)) {
              r = 0;
              break;
            }
            cl = clLvalue(fv);
            if (-3 - enc < 0 || -3 - enc >= cl->p->sizeupvalues) {
              r = 0;
              break;
            }
            val = cl->upvals[-3 - enc]->v.p;
          }
          r = table_fill_any_intrinsic(
                  L, (TValue *)base, expected_base,
                  hvalue(&((TValue *)base)[denc]), val,
                  p->luar_tint_upval[i]);
          break;
        }
        case 58:
          if (!ttistable(&((TValue *)base)[p->luar_tint_table[i]])) {
            r = 0;
            break;
          }
          r = table_nil_clear(L, (TValue *)base, expected_base,
                              hvalue(&((TValue *)base)[p->luar_tint_table[i]]));
          break;
        case 57: {
          int denc = p->luar_tint_table[i];
          int senc = p->luar_tint_keys[i];
          TValue *srcv = pair_source_value((TValue *)base,
                                           (senc >= 0) ? senc : -1,
                                           (senc >= 0) ? -1 : (-1 - senc));
          if (srcv == NULL || !ttistable(&((TValue *)base)[denc])) {
            r = 0;
            break;
          }
          r = table_copy_to_intrinsic(
                  L, (TValue *)base, expected_base,
                  hvalue(&((TValue *)base)[denc]), srcv,
                  p->luar_tint_upval[i]);
          break;
        }
        case 51:
          r = table_nil_store_intrinsic(L, (TValue *)base, expected_base,
                                        p->luar_tint_table[i]);
          break;
        case 54:
          r = table_pair_store_intrinsic(
                  L, (TValue *)base, expected_base,
                  p->luar_tint_table[i], p->luar_tint_keys[i],
                  p->luar_tint_upval[i], p->luar_tint_acc[i],
                  p->luar_tint_res[i] & 0xff,
                  (p->luar_tint_res[i] >> 8) & 0xff,
                  p->luar_tint_arg4[i]);
          break;
        case 56: {
          int senc = p->luar_tint_keys[i];
          TValue *srcv = pair_source_value((TValue *)base,
                                           (senc >= 0) ? senc : -1,
                                           (senc >= 0) ? -1 : (-1 - senc));
          if (srcv == NULL) {
            r = 0;
            break;
          }
          r = luar_ffi_trace_unary_store(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  srcv,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  &((TValue *)base)[p->luar_tint_res[i]],
                  p->luar_tint_acc[i]);
          break;
        }
        case 29:
          r = table_affine_fill_intrinsic(L, (TValue *)base, expected_base,
                                          p->luar_tint_table[i],
                                          p->luar_tint_keys[i],
                                          p->luar_tint_upval[i]);
          break;
        case 10: {
          Table *h = tint_upval_table(p, (TValue *)base, i);
          r = (h == NULL) ? 0
              : table_sum_table(L, (TValue *)base, expected_base, h,
                                p->luar_tint_acc[i], p->luar_tint_res[i],
                                0);
          break;
        }
        case 11:
          r = table_sum3_intrinsic(L, (TValue *)base, expected_base,
                                   p->luar_tint_table[i],
                                   p->luar_tint_keys[i],
                                   p->luar_tint_upval[i],
                                   p->luar_tint_acc[i],
                                   p->luar_tint_res[i],
                                   p->luar_tint_arg4[i] & 0xff,
                                   (p->luar_tint_arg4[i] >> 8) & 0xff);
          break;
        case 12:
        case 13:
        case 14:
        case 36:
        case 38:
        case 40:
        case 43:
        case 44:
        case 45:
        case 46: {
          int aenc = p->luar_tint_table[i];
          int benc = p->luar_tint_keys[i];
          int areg = (aenc >= 0) ? aenc : -1;
          int aup = (aenc >= 0) ? -1 : (-1 - aenc);
          int breg = (benc >= 0) ? benc : -1;
          int bup = (benc >= 0) ? -1 : (-1 - benc);
          r = pair_array_intrinsic(L, (TValue *)base, expected_base,
                                   areg, aup, breg, bup,
                                   p->luar_tint_acc[i],
                                   p->luar_tint_res[i],
                                   p->luar_tint_upval[i],
                                   p->luar_tint_arg4[i], kind);
          break;
        }
        case 15:
        case 28:
        case 32:
        case 33: {
          TValue *sc = tint_scalar(p, (TValue *)base,
                                   p->luar_tint_keys[i]);
          Table *h = pair_source_table((TValue *)base,
                                       (p->luar_tint_table[i] >= 0)
                                         ? p->luar_tint_table[i] : -1,
                                       (p->luar_tint_table[i] >= 0)
                                         ? -1 : (-1 - p->luar_tint_table[i]));
          r = (sc == NULL || h == NULL) ? 0
              : scaled_array_intrinsic(L, (TValue *)base, expected_base, h,
                                       sc, p->luar_tint_upval[i],
                                       p->luar_tint_arg4[i],
                                       p->luar_tint_acc[i],
                                       kind == 28 || kind == 33,
                                       kind == 32 || kind == 33);
          break;
        }
        case 30:
        case 31:
        case 34:
        case 35: {
          int enc = p->luar_tint_table[i];
          TValue *av = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          TValue *sc = tint_scalar(p, (TValue *)base,
                                   p->luar_tint_keys[i]);
          r = (av == NULL || sc == NULL) ? 0
              : luar_ffi_trace_scaled_array(
                    av, sc,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &((TValue *)base)[p->luar_tint_acc[i]],
                    &((TValue *)base)[p->luar_tint_upval[i]],
                    &((TValue *)base)[p->luar_tint_res[i]],
                    kind == 31 || kind == 35,
                    kind == 34 || kind == 35);
          break;
        }
        case 16: {
          int enc = p->luar_tint_table[i];
          TValue *av = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          r = (av == NULL) ? 0
              : luar_ffi_trace_sum_array(
                    av,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &((TValue *)base)[p->luar_tint_acc[i]],
                    &((TValue *)base)[p->luar_tint_res[i]],
                    p->luar_tint_arg4[i]);
          break;
        }
        case 17:
        case 18:
        case 19:
        case 37:
        case 39:
        case 41:
        case 47:
        case 48:
        case 49:
        case 50: {
          int aenc = p->luar_tint_table[i];
          int benc = p->luar_tint_keys[i];
          TValue *av = pair_source_value((TValue *)base,
                                         (aenc >= 0) ? aenc : -1,
                                         (aenc >= 0) ? -1 : (-1 - aenc));
          TValue *bv = pair_source_value((TValue *)base,
                                         (benc >= 0) ? benc : -1,
                                         (benc >= 0) ? -1 : (-1 - benc));
          r = (av == NULL || bv == NULL) ? 0
              : luar_ffi_trace_pair_array(
                    av, bv,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &((TValue *)base)[p->luar_tint_acc[i]],
                    &((TValue *)base)[p->luar_tint_upval[i]],
                    &((TValue *)base)[p->luar_tint_arg4[i]],
                    &((TValue *)base)[p->luar_tint_res[i]],
                    (kind == 19) ? 2 :
                    (kind == 37) ? 3 :
                    (kind == 39) ? 4 :
                    (kind == 41) ? 5 :
                    (kind == 47) ? 6 :
                    (kind == 48) ? 7 :
                    (kind == 49) ? 8 :
                    (kind == 50) ? 9 : kind - 17);
          break;
        }
        case 25: {
          int enc = p->luar_tint_table[i];
          TValue *av = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          if (av != NULL) {
            int mode = p->luar_tint_keys[i];
            int temp = p->luar_tint_upval[i];
            TValue *val = (mode == -2) ? &p->k[p->luar_tint_arg4[i]]
                        : (mode >= 0) ? &((TValue *)base)[mode] : NULL;
            ((TValue *)base)[temp] = *av;
            r = luar_ffi_trace_store_array(
                    av,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    val, (mode == -1) ? 1 : (mode == -2) ? 2 : 0);
          }
          break;
        }
        case 27: {
          int enc = p->luar_tint_table[i];
          TValue *av = pair_source_value((TValue *)base,
                                         (enc >= 0) ? enc : -1,
                                         (enc >= 0) ? -1 : (-1 - enc));
          if (av != NULL) {
            int senc = p->luar_tint_keys[i];
            int op = p->luar_tint_upval[i];
            TValue scalar;
            if (op >= 11) {
              setivalue(&scalar, 0);   /* unary ops ignore the operand */
            }
            else if (senc == -2) {
              setivalue(&scalar, (lua_Integer)p->luar_tint_arg4[i]);
            }
            else if (senc == -1)
              scalar = p->k[p->luar_tint_arg4[i]];
            else
              scalar = ((TValue *)base)[senc];
            ((TValue *)base)[p->luar_tint_acc[i]] = *av;
            r = luar_ffi_trace_update_array(
                    av,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &scalar,
                    &((TValue *)base)[p->luar_tint_res[i]],
                    op);
          }
          break;
        }
        case 20: {
          int arr = p->luar_tint_table[i];
          int enc = p->luar_tint_keys[i];
          int mode = (enc == -1) ? 1 : (enc == -2) ? 2 : 0;
          TValue *val = (mode == 2) ? &p->k[p->luar_tint_arg4[i]]
                       : (mode == 0) ? &((TValue *)base)[enc] : NULL;
          r = luar_ffi_trace_store_array(
                  &((TValue *)base)[arr],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1], val, mode);
          break;
        }
        case 21: {
          int senc = p->luar_tint_keys[i];
          TValue *srcv = pair_source_value((TValue *)base,
                                           (senc >= 0) ? senc : -1,
                                           (senc >= 0) ? -1 : (-1 - senc));
          r = (srcv == NULL) ? 0
              : luar_ffi_trace_copy_array(
                    &((TValue *)base)[p->luar_tint_table[i]],
                    srcv,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &((TValue *)base)[p->luar_tint_acc[i]]);
          break;
        }
        case 22: {
          TValue scalar;
          int enc = p->luar_tint_keys[i];
          int op = p->luar_tint_upval[i];
          if (op >= 11) {
            setivalue(&scalar, 0);   /* unary ops ignore the operand */
          }
          else if (enc == -2) {
            setivalue(&scalar, (lua_Integer)p->luar_tint_arg4[i]);
          }
          else if (enc == -1)
            scalar = p->k[p->luar_tint_arg4[i]];
          else
            scalar = ((TValue *)base)[enc];
          r = luar_ffi_trace_update_array(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &scalar,
                  &((TValue *)base)[p->luar_tint_acc[i]],
                  op);
          break;
        }
        case 26: {
          int obj = p->luar_tint_table[i];
          int keyidx = p->luar_tint_keys[i];
          int tmp = p->luar_tint_upval[i];
          TValue *ov = &((TValue *)base)[obj];
          if (ttistable(ov)) {
            TValue field;
            memset(&field, 0, sizeof(field));
            if (luaH_getstr(hvalue(ov), tsvalue(&p->k[keyidx]), &field) !=
                LUA_TNIL && ttistable(&field))
              r = table_sum_table(L, (TValue *)base, expected_base,
                                  hvalue(&field), p->luar_tint_acc[i], tmp,
                                  0);
          }
          break;
        }
        case 23: {
          int aenc = p->luar_tint_keys[i];
          int benc = p->luar_tint_upval[i];
          TValue *av = pair_source_value((TValue *)base,
                                         (aenc >= 0) ? aenc : -1,
                                         (aenc >= 0) ? -1 : (-1 - aenc));
          TValue *bv = pair_source_value((TValue *)base,
                                         (benc >= 0) ? benc : -1,
                                         (benc >= 0) ? -1 : (-1 - benc));
          r = (av == NULL || bv == NULL) ? 0
              : luar_ffi_trace_pair_store(
                    &((TValue *)base)[p->luar_tint_table[i]],
                    av, bv,
                    &((TValue *)base)[expected_base + 2],
                    &((TValue *)base)[expected_base],
                    &((TValue *)base)[expected_base + 1],
                    &((TValue *)base)[p->luar_tint_acc[i]],
                    &((TValue *)base)[p->luar_tint_res[i]],
                    p->luar_tint_arg4[i]);
          break;
        }
        case 24: {
          TValue scalar;
          TValue *srcv;
          int senc = p->luar_tint_keys[i];
          int enc = p->luar_tint_upval[i];
          srcv = pair_source_value((TValue *)base,
                                   (senc >= 0) ? senc : -1,
                                   (senc >= 0) ? -1 : (-1 - senc));
          if (srcv == NULL) {
            r = 0;
            break;
          }
          if (enc == -2) {
            setivalue(&scalar, (lua_Integer)p->luar_tint_arg4[i]);
          }
          else if (enc == -1)
            scalar = p->k[p->luar_tint_arg4[i]];
          else if (enc <= -3) {
            int up = -3 - enc;
            TValue *fv = &((TValue *)base)[-1];
            LClosure *cl;
            if (!ttisLclosure(fv)) {
              r = 0;
              break;
            }
            cl = clLvalue(fv);
            if (up < 0 || up >= cl->p->sizeupvalues) {
              r = 0;
              break;
            }
            scalar = cl->upvals[up]->v.p[0];
          }
          else
            scalar = ((TValue *)base)[enc];
          r = luar_ffi_trace_scalar_store(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  srcv,
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &scalar,
                  &((TValue *)base)[p->luar_tint_res[i]],
                  p->luar_tint_acc[i]);
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
        default:
          /* An unknown kind is a bug, not a keyed sum.  Falling through to
          ** a helper is how the keyed-sum/affine-fill kind collision went
          ** unnoticed; fail closed instead. */
          r = 0;
          break;
      }
      if (r == 0) {
        J->exits++;
        /* LUAR: an intrinsic that cannot cover this iteration (out-of-range
        ** call, non-array shape, metatable, short table, size cap) would
        ** fail again at every backedge.  Suspend it only for this CallInfo,
        ** so the rest of the current invocation runs on the interpreter but
        ** the next invocation of the same loop retries.  A single transient
        ** out-of-bounds call must not permanently disable a loop that is
        ** native for every normal input.  A shape the intrinsic can never
        ** handle pays one cheap helper attempt per call, not per
        ** iteration. */
        p->luar_tint_suspend[i] = L->ci->luar_gen;
      }
      else {
        p->luar_tint_suspend[i] = 0;
        J->tint_hits++;   /* LUAR: observable proof the intrinsic ran */
      }
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
      if (p->luar_fld_type[i] != 0) {
        TValue *tv = &((TValue *)base)[p->luar_fld_table[i]];
        TValue cur;
        int ok = 0;
        if (ttistable(tv) && p->luar_fld_key[i] >= 0 &&
            p->luar_fld_key[i] < p->sizek &&
            ttisstring(&p->k[p->luar_fld_key[i]])) {
          memset(&cur, 0, sizeof(cur));
          if (luaH_getstr(hvalue(tv), tsvalue(&p->k[p->luar_fld_key[i]]),
                          &cur) != LUA_TNIL) {
            if (p->luar_fld_type[i] == LUAR_T_INT) {
              ok = ttisinteger(&cur) &&
                   ivalue(&cur) == p->luar_fld_imm[i];
            }
            else if (ttisfloat(&cur)) {
              double v = fltvalue(&cur);
              ok = memcmp(&v, &p->luar_fld_num[i], sizeof(v)) == 0;
            }
          }
        }
        if (!ok) {
          p->luar_code[i] = NULL;
          p->luar_code_epoch[i] = 0;
          p->luar_fld_type[i] = 0;
          J->exits++;
          continue;
        }
      }
      any = 1;
      r = luari_run_j4(p, p->luar_code[i], base, L);
      if (r != 0) {
        if (r < -1) {
          int expc = -r - 2;
          J->last_exit_pc = expc;
          /* An exit at the loop entry is the ordinary tag-guard exit that
          ** drives polymorphic variants.  A later exit (a mid-body guard)
          ** cannot be retried safely: re-entering the loop would hit the
          ** same guard after every backedge.  Drop this native loop and let
          ** the interpreter finish it. */
          if (expc != p->luar_code_start[i]) {
            p->luar_code[i] = NULL;
            p->luar_code_epoch[i] = 0;
            return r;
          }
          if (!p->luar_ir_side_done) {
            LuaR_TraceIR ir;
            p->luar_ir_side_done = 1;
            if (expc >= 0 && expc <= pc &&
                luar_trace_record_loop(p, expc, pc, (TValue *)base, L, &ir)) {
              int link;
              if (!ir_side_allows(p, (TValue *)base, &ir)) {
                J->ir_side_aborts++;
                goto side_done;
              }
              link = luar_trace_ir_link(&ir, 0,
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
          side_done:
            ;
          }
          if (p->luar_side_code != NULL &&
              p->luar_side_epoch == J->epoch &&
              p->luar_side_pc == pc && p->luar_side_start == expc) {
            LuaR_IRExitState snap;
            int ns = p->maxstacksize;
            if (ns > LUAR_IR_MAX_SLOTS) ns = LUAR_IR_MAX_SLOTS;
            if (luar_trace_ir_snapshot(&snap, expc, (TValue *)base, ns)) {
              int sr = luari_run_j4(p, p->luar_side_code, base, L);
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

/* LUAR: after the float pass, verify that every remaining integer
** arithmetic instruction really has integer operands.  The old pass
** converted only the chain rooted at the first float it saw; a later
** independent `t = t * 1` or `u = s * -1` stayed an integer MULK while
** its operand was a double, and the emitter then reinterpreted the double
** payload.  Rejecting the trace keeps the interpreter authoritative.
*/
static int ir_float_consistent (const LuaR_TraceIR *ir, TValue *base) {
  int type[LUAR_IR_MAX_SLOTS];
  int i, ns = ir->maxslots;
  if (ns > LUAR_IR_MAX_SLOTS) ns = LUAR_IR_MAX_SLOTS;
  for (i = 0; i < ns; i++)
    type[i] = ttisinteger(&base[i]) ? 1
              : ttisfloat(&base[i]) ? 2 : 0;
  for (i = 0; i < ir->nins; i++) {
    const LuaR_IRIns *in = &ir->ins[i];
    switch (in->kind) {
      case LUAR_IR_GUARD:
        if (in->b == LUAR_IR_TAG_INT) type[in->a] = 1;
        else if (in->b == LUAR_IR_TAG_FLOAT) type[in->a] = 2;
        break;
      case LUAR_IR_KINT:
      case LUAR_IR_LOADI:
        type[in->a] = 1;
        break;
      case LUAR_IR_LOADF:
        type[in->a] = 2;
        break;
      case LUAR_IR_SLOAD:
        /* The runtime value is copied from a slot, not a compile-time
        ** constant; a later guard establishes the type. */
        type[in->a] = 0;
        break;
      case LUAR_IR_ULOAD:
        type[in->a] = 0;
        break;
      case LUAR_IR_ADDI:
        if (type[in->b] != 1) return 0;
        type[in->a] = 1;
        break;
      case LUAR_IR_ADDK:
      case LUAR_IR_SUBK:
      case LUAR_IR_MODK:
      case LUAR_IR_IDIVK:
        /* No float variants exist for these K operations. */
        if (type[in->b] != 1 || in->kfloat) return 0;
        type[in->a] = 1;
        break;
      case LUAR_IR_MULK:
        if (in->kfloat) {
          if (type[in->b] == 0 || type[in->b] > 2) return 0;
          type[in->a] = 2;
        }
        else {
          if (type[in->b] != 1) return 0;
          type[in->a] = 1;
        }
        break;
      case LUAR_IR_ADD:
      case LUAR_IR_SUB:
      case LUAR_IR_MUL:
      case LUAR_IR_DIV:
        if (type[in->b] != 1 || type[in->c] != 1) return 0;
        type[in->a] = 1;
        break;
      case LUAR_IR_FMULK:
      case LUAR_IR_FDIVK:
        if (type[in->b] == 0 || type[in->b] > 2) return 0;
        type[in->a] = 2;
        break;
      case LUAR_IR_FADDI:
        if (type[in->b] != 2) return 0;
        type[in->a] = 2;
        break;
      case LUAR_IR_FADD:
      case LUAR_IR_FSUB:
        /* Mixed int/float operands are converted by the runtime helper;
        ** both sides must still be numeric.  A both-int FADD/FSUB is not
        ** produced by ir_floatify, but reject it here if it ever is. */
        if (type[in->b] == 0 || type[in->b] > 2 ||
            type[in->c] == 0 || type[in->c] > 2)
          return 0;
        if (type[in->b] == 1 && type[in->c] == 1)
          return 0;
        type[in->a] = 2;
        break;
      case LUAR_IR_ITOF:
        type[in->a] = 2;
        break;
      case LUAR_IR_TGET:
      case LUAR_IR_TGETI:
      case LUAR_IR_TGETK:
        type[in->a] = 1;
        break;
      case LUAR_IR_TGETF:
      case LUAR_IR_TGETKF:
        type[in->a] = 2;
        break;
      /* LUAR: cdata reads are producers too.  Without these cases an
      ** otherwise valid mixed FFI trace had an unknown operand type and
      ** was accepted by the old ir_has_ffi_ops() shortcut even when a
      ** float guard met an integer ADD/MUL (or the reverse). */
      case LUAR_IR_CGETI:
      case LUAR_IR_CPGETI:
        type[in->a] = 1;
        break;
      case LUAR_IR_CGETF:
      case LUAR_IR_CPGETF:
        type[in->a] = 2;
        break;
      case LUAR_IR_CGETFIELD:
        type[in->a] = (in->c == 2) ? 2 : 1;
        break;
      case LUAR_IR_CCALL0I:
      case LUAR_IR_CCALL1I:
      case LUAR_IR_CCALL2II:
        type[in->a] = 1;
        break;
      case LUAR_IR_CCALL0D:
      case LUAR_IR_CCALL1DI:
      case LUAR_IR_CCALL1DD:
      case LUAR_IR_CCALL1FF:
      case LUAR_IR_CCALL2DD:
      case LUAR_IR_CCALL2FF:
      case LUAR_IR_CCALL2FI:
      case LUAR_IR_CCALL2IF:
      case LUAR_IR_CCALL2FD:
      case LUAR_IR_CCALL2DF:
      case LUAR_IR_CCALL2DI:
      case LUAR_IR_CCALL2ID:
      case LUAR_IR_CCALL3DD:
      case LUAR_IR_CCALL3DDI:
      case LUAR_IR_CCALL3DID:
      case LUAR_IR_CCALL3IDD:
        type[in->a] = 2;
        break;
      default:
        break;
    }
  }
  return 1;
}


/* Whether a recorded dynamic GETTABLE yields a float in the current
** stack.  Floatification uses the same test; this helper lets the caller
** decide whether an otherwise integer-looking trace still needs it. */
static int ir_tget_is_float (Proto *p, TValue *base,
                             const LuaR_IRIns *op) {
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
  return luar_trace_tgetf_generic(&base[tb], ivalue(&base[tk]), &fv);
}


static int ir_has_float_get (Proto *p, TValue *base,
                             const LuaR_TraceIR *ir) {
  int i;
  for (i = 0; i < ir->nins; i++)
    if (ir->ins[i].kind == LUAR_IR_TGET &&
        ir_tget_is_float(p, base, &ir->ins[i]))
      return 1;
  return 0;
}


/* LUAR: the type a slot holds just before instruction 'at'.  The trace is
** a linear sequence, so replay the producers up to that point.  Keeping
** this local to the float pass lets ADD/SUB use the runtime helpers for a
** mixed operand without rewriting the source slot. */
static int ir_slot_type_at (const LuaR_TraceIR *ir, TValue *base,
                            int slot, int at) {
  int typ[LUAR_IR_MAX_SLOTS];
  int i, ns = ir->maxslots;
  if (slot < 0 || slot >= LUAR_IR_MAX_SLOTS)
    return 0;
  if (ns > LUAR_IR_MAX_SLOTS) ns = LUAR_IR_MAX_SLOTS;
  for (i = 0; i < LUAR_IR_MAX_SLOTS; i++)
    typ[i] = (i < ns) ? (ttisinteger(&base[i]) ? 1
                        : ttisfloat(&base[i]) ? 2 : 0) : 0;
  for (i = 0; i < at && i < ir->nins; i++) {
    const LuaR_IRIns *in = &ir->ins[i];
    int a = in->a;
    if (a >= LUAR_IR_MAX_SLOTS)
      continue;
    switch (in->kind) {
      case LUAR_IR_GUARD:
        typ[a] = (in->b == LUAR_IR_TAG_INT) ? 1
                 : (in->b == LUAR_IR_TAG_FLOAT) ? 2
                 : (in->b == LUAR_IR_TAG_TABLE) ? 3 : 0;
        break;
      case LUAR_IR_KINT:
      case LUAR_IR_LOADI:
      case LUAR_IR_ADDI:
      case LUAR_IR_ADDK:
      case LUAR_IR_SUBK:
      case LUAR_IR_MODK:
      case LUAR_IR_IDIVK:
      case LUAR_IR_ADD:
      case LUAR_IR_SUB:
      case LUAR_IR_MUL:
      case LUAR_IR_DIV:
      case LUAR_IR_TGET:
      case LUAR_IR_TGETI:
      case LUAR_IR_TGETK:
      case LUAR_IR_CGETI:
      case LUAR_IR_CPGETI:
      case LUAR_IR_CCALL0I:
      case LUAR_IR_CCALL1I:
      case LUAR_IR_CCALL2II:
        typ[a] = 1;
        break;
      case LUAR_IR_KFLOAT:
      case LUAR_IR_LOADF:
      case LUAR_IR_ITOF:
      case LUAR_IR_FADDI:
      case LUAR_IR_FMULK:
      case LUAR_IR_FADD:
      case LUAR_IR_FSUB:
      case LUAR_IR_FDIVK:
      case LUAR_IR_DIVK:
      case LUAR_IR_TGETF:
      case LUAR_IR_TGETKF:
      case LUAR_IR_CGETF:
      case LUAR_IR_CPGETF:
      case LUAR_IR_CCALL0D:
      case LUAR_IR_CCALL1DI:
      case LUAR_IR_CCALL1DD:
      case LUAR_IR_CCALL1FF:
      case LUAR_IR_CCALL2DD:
      case LUAR_IR_CCALL2FF:
      case LUAR_IR_CCALL2FI:
      case LUAR_IR_CCALL2IF:
      case LUAR_IR_CCALL2FD:
      case LUAR_IR_CCALL2DF:
      case LUAR_IR_CCALL2DI:
      case LUAR_IR_CCALL2ID:
      case LUAR_IR_CCALL3DD:
      case LUAR_IR_CCALL3DDI:
      case LUAR_IR_CCALL3DID:
      case LUAR_IR_CCALL3IDD:
        typ[a] = 2;
        break;
      case LUAR_IR_MULK:
        typ[a] = in->kfloat ? 2 : 1;
        break;
      case LUAR_IR_CGETFIELD:
        typ[a] = (in->c == 2) ? 2 : 1;
        break;
      case LUAR_IR_SLOAD:
        typ[a] = 0;
        break;
      case LUAR_IR_ULOAD:
        typ[a] = 0;
        break;
      default:
        break;
    }
  }
  return typ[slot];
}


static int ir_floatify (Proto *p, TValue *base, LuaR_TraceIR *ir) {
  int i, fslot = -1, sawtable = 0, sawfloat = 0;
  for (i = 0; i < ir->nins; i++) {
    if (ir->ins[i].kind == LUAR_IR_CPGETI64 ||
        ir->ins[i].kind == LUAR_IR_CPSETI64)
      return 0;
    if (ir->ins[i].kind == LUAR_IR_CPGETI64 ||
        ir->ins[i].kind == LUAR_IR_CPSETI64)
      return 0;
    LuaR_IRIns *op = &ir->ins[i];
    if (op->kind == LUAR_IR_GUARD) {
      if (op->b != LUAR_IR_TAG_INT && op->b != LUAR_IR_TAG_FLOAT &&
          op->b != LUAR_IR_TAG_TABLE && op->b != LUAR_IR_TAG_STRING &&
          op->b != LUAR_IR_TAG_USERDATA)
        return 0;
      if (op->b == LUAR_IR_TAG_FLOAT)
        sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_GUARD && op->b == LUAR_IR_TAG_FLOAT) {
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_ADDI ||
        op->kind == LUAR_IR_LOADI || op->kind == LUAR_IR_SLOAD ||
        op->kind == LUAR_IR_ULOAD)
      continue;
    if (op->kind == LUAR_IR_KSTR || op->kind == LUAR_IR_KBOOL)
      continue;
    if (op->kind == LUAR_IR_TSETI ||
        op->kind == LUAR_IR_TSETIF ||
        op->kind == LUAR_IR_TSETIS ||
        op->kind == LUAR_IR_TSETIB ||
        op->kind == LUAR_IR_TSETIT) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_ITOF) {
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_FADDI) {
      fslot = op->a;
      sawfloat = 1;
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_LOADF) {
      /* A constant slot is not a flowing float producer: leave fslot to a
      ** CCALL1DD (or another float op) so a later reused-slot ADD cannot be
      ** misclassified as a float add. */
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_CCALL0D) {
      fslot = op->a;
      sawfloat = 1;
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_CCALL1DI ||
        op->kind == LUAR_IR_CCALL1DD ||
        op->kind == LUAR_IR_CCALL1FF ||
        op->kind == LUAR_IR_CCALL2DD ||
        op->kind == LUAR_IR_CCALL2FF ||
        op->kind == LUAR_IR_CCALL2FI ||
        op->kind == LUAR_IR_CCALL2IF ||
        op->kind == LUAR_IR_CCALL2FD ||
        op->kind == LUAR_IR_CCALL2DF ||
        op->kind == LUAR_IR_CCALL2DI ||
        op->kind == LUAR_IR_CCALL2ID ||
        op->kind == LUAR_IR_CCALL3DD ||
        op->kind == LUAR_IR_CCALL3DDI ||
        op->kind == LUAR_IR_CCALL3DID ||
        op->kind == LUAR_IR_CCALL3IDD) {
      fslot = op->a;
      sawfloat = 1;
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TGETKF) {
      fslot = op->a;
      sawfloat = 1;
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_CGETF || op->kind == LUAR_IR_CPGETF ||
        (op->kind == LUAR_IR_CGETFIELD && op->c == 2)) {
      fslot = op->a;
      sawfloat = 1;
      sawtable = 1;
      continue;
    }
    /* Integer cdata reads may coexist with float ones in the same trace
    ** (s += a[i] over int[] while x += d[i] over double[]); they just do
    ** not take part in the float chain. */
    if (op->kind == LUAR_IR_CGETI || op->kind == LUAR_IR_CPGETI ||
        (op->kind == LUAR_IR_CGETFIELD && op->c != 2)) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TGET) {
      if (ir_tget_is_float(p, base, op)) {
        op->kind = LUAR_IR_TGETF;
        fslot = op->a;
        sawfloat = 1;
      }
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_MULK) {
      if ((sawfloat && op->b == fslot) || op->kfloat) {
        op->kind = LUAR_IR_FMULK;
        fslot = op->a;
        sawfloat = 1;
        continue;
      }
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_ADDK || op->kind == LUAR_IR_SUBK ||
        op->kind == LUAR_IR_MODK || op->kind == LUAR_IR_IDIVK) {
      /* LUAR: there is no float form of these K ops.  A float constant is
      ** still recorded as an integer kind with ival == 0; emitting it would
      ** compute with 0 and retag the result as an integer.  Reject so the
      ** side trace falls back to the interpreter instead of miscompiling. */
      if (op->kfloat)
        return 0;
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_CSETF && sawfloat && op->c == fslot) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TSETUPKV) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TSETUPKF) {
      sawtable = 1;
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TSETDYN_KB) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TSETDYN_KI) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TSETDYN_KS) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TSETDYN_KF) {
      sawtable = 1;
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TSETDYN) {
      sawtable = 1;
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TSETKF ||
        op->kind == LUAR_IR_TSETKKF ||
        op->kind == LUAR_IR_TSETKKS) {
      sawtable = 1;
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_TNEW) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_CPSETF) {
      sawtable = 1;
      sawfloat = 1;
      continue;
    }
    if (op->kind == LUAR_IR_CSETFIELD && sawfloat && op->b == fslot) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_CSETFIELDK && op->c == 2) {
      sawtable = 1;
      sawfloat = 1;
      continue;
    }
    if ((op->kind == LUAR_IR_ADD || op->kind == LUAR_IR_SUB) &&
        sawfloat) {
      int bt = ir_slot_type_at(ir, base, op->b, i);
      int ct = ir_slot_type_at(ir, base, op->c, i);
      /* The runtime FADD/FSUB helpers read both operands with Lua's mixed
      ** numeric conversion, exactly like the interpreter, and write only
      ** the result.  Do not manufacture an in-place ITOF: the integer
      ** operand can be a visible local or a FORLOOP control slot, and
      ** converting it would corrupt the loop state or the local's type. */
      if ((bt == 2 || ct == 2) &&
          (bt == 1 || bt == 2) && (ct == 1 || ct == 2)) {
        op->kind = (op->kind == LUAR_IR_ADD) ? LUAR_IR_FADD
                                             : LUAR_IR_FSUB;
        fslot = op->a;
        sawfloat = 1;
        continue;
      }
      if (bt != 1 || ct != 1)
        return 0;              /* unknown operand type: stay interpreted */
    }
    if (op->kind == LUAR_IR_DIVK) {
      /* Lua '/' always produces a float, even for two integers. */
      op->kind = LUAR_IR_FDIVK;
      fslot = op->a;
      sawfloat = 1;
      continue;
    }
    /* Integer arithmetic that is not part of the float chain stays as an
    ** integer op; a mixed trace may legitimately contain both chains. */
    if (op->kind == LUAR_IR_ADD || op->kind == LUAR_IR_SUB ||
        op->kind == LUAR_IR_MUL || op->kind == LUAR_IR_DIV) {
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_FORLOOP || op->kind == LUAR_IR_RET ||
        op->kind == LUAR_IR_EXIT || op->kind == LUAR_IR_CGUARD)
      continue;
    return 0;
  }
  /* LUAR: every floatified trace must also pass the operand-type check,
  ** including traces that contain FFI ops.  The old ir_has_ffi_ops()
  ** bypass let a float guard coexist with an integer ADD/MUL over the
  ** same slot; the emitter then reinterpreted the double payload.  A
  ** trace that cannot be made consistent is rejected and J2/interpreter
  ** stays authoritative. */
  return sawtable && sawfloat && ir_float_consistent(ir, base);
}


static int ir_side_allows (Proto *p, TValue *base, LuaR_TraceIR *ir) {
  /* LUAR: always analyse the side trace.  The old check skipped
  ** 'ir_floatify' when the *root* trace was integer-only, so a side trace
  ** with a float-producing K op (but no float GUARD) slipped through and
  ** the integer emitter used ival == 0.  If the trace is not a supported
  ** float trace it must be provably integer-only. */
  if (ir_floatify(p, base, ir))
    return 1;
  if (!ir_integer_only(ir))
    return 0;
  if (ir_has_float_get(p, base, ir))
    return 0;   /* dynamic read would be truncated by integer emission */
  return 1;
}


/* A known string/userdata/table/boolean slot cannot be used by the raw
** integer arithmetic emitters: the interpreter would call a metamethod or
** raise.  Unknown (-1) slots are left to the recorder's guards. */
static int ir_bad_arith_tag (int tag) {
  return tag == LUAR_IR_TAG_TABLE || tag == LUAR_IR_TAG_OTHER ||
         tag == LUAR_IR_TAG_STRING || tag == LUAR_IR_TAG_USERDATA;
}


static int ir_integer_only (const LuaR_TraceIR *ir) {
  int i, tag[LUAR_IR_MAX_SLOTS];
  for (i = 0; i < ir->nins; i++)
    if (ir->ins[i].kind == LUAR_IR_ITOF ||
        ir->ins[i].kind == LUAR_IR_FADDI ||
        ir->ins[i].kind == LUAR_IR_FADD ||
        ir->ins[i].kind == LUAR_IR_FSUB ||
        ir->ins[i].kind == LUAR_IR_FMULK ||
        ir->ins[i].kind == LUAR_IR_FDIVK ||
        ir->ins[i].kind == LUAR_IR_TGETF ||
        ir->ins[i].kind == LUAR_IR_TGETKF ||
        ir->ins[i].kind == LUAR_IR_DIVK ||
        ir->ins[i].kfloat ||
        ir->ins[i].kind == LUAR_IR_CPGETI64 ||
        ir->ins[i].kind == LUAR_IR_CPSETI64)
      return 0;
  for (i = 0; i < ir->nins; i++)
    if (ir->ins[i].kind == LUAR_IR_CCPYI64 ||
        ir->ins[i].kind == LUAR_IR_CADDI64)
      continue;
  for (i = 0; i < LUAR_IR_MAX_SLOTS; i++)
    tag[i] = -1;
  for (i = 0; i < ir->nins; i++) {
    const LuaR_IRIns *in = &ir->ins[i];
    if (in->kind == LUAR_IR_CGETF || in->kind == LUAR_IR_CPGETF ||
        in->kind == LUAR_IR_CCALL1DI || in->kind == LUAR_IR_CCALL1DD ||
        in->kind == LUAR_IR_CCALL1FF ||
        in->kind == LUAR_IR_CCALL2DD ||
        in->kind == LUAR_IR_CCALL2FF || in->kind == LUAR_IR_CCALL3DD ||
        in->kind == LUAR_IR_LOADF || in->kind == LUAR_IR_FADDI ||
        in->kind == LUAR_IR_TSETKF ||
        in->kind == LUAR_IR_TGETKF ||
        in->kind == LUAR_IR_TSETKKF ||
        in->kind == LUAR_IR_CPSETF ||
        ((in->kind == LUAR_IR_CGETFIELD ||
          in->kind == LUAR_IR_CSETFIELD ||
          in->kind == LUAR_IR_CSETFIELDK) && in->c == 2))
      return 0;
    if (in->kind == LUAR_IR_GUARD) {
      if (in->b != LUAR_IR_TAG_INT && in->b != LUAR_IR_TAG_TABLE &&
          in->b != LUAR_IR_TAG_STRING &&
          in->b != LUAR_IR_TAG_USERDATA)
        return 0;
      tag[in->a] = (uint8_t)in->b;
    }
    else if (in->kind == LUAR_IR_TGETK || in->kind == LUAR_IR_TGETI)
      tag[in->a] = LUAR_IR_TAG_INT;
    else if (in->kind == LUAR_IR_TGETKT)
      tag[in->a] = LUAR_IR_TAG_TABLE;
    else if (in->kind == LUAR_IR_TGETKS)
      tag[in->a] = LUAR_IR_TAG_STRING;
    else if (in->kind == LUAR_IR_TGETKB)
      tag[in->a] = LUAR_IR_TAG_OTHER;   /* boolean has no arithmetic */
    else if (in->kind == LUAR_IR_TGET)
      tag[in->a] = -1;   /* dynamic value; following guards decide */
  }
  for (i = 0; i < ir->nins; i++) {
    const LuaR_IRIns *in = &ir->ins[i];
    if (in->kind == LUAR_IR_ADD || in->kind == LUAR_IR_SUB ||
        in->kind == LUAR_IR_MUL || in->kind == LUAR_IR_DIV) {
      if (ir_bad_arith_tag(tag[in->b]) || ir_bad_arith_tag(tag[in->c]))
        return 0;
    }
    if ((in->kind == LUAR_IR_ADDI ||
         (in->kind >= LUAR_IR_ADDK && in->kind <= LUAR_IR_IDIVK)) &&
        ir_bad_arith_tag(tag[in->b]))
      return 0;
    if (in->kind >= LUAR_IR_ADDK && in->kind <= LUAR_IR_IDIVK &&
        in->kfloat)
      return 0;
  }
  return 1;
}


static int ir_has_tnew (const LuaR_TraceIR *ir) {
  int i;
  for (i = 0; i < ir->nins; i++)
    if (ir->ins[i].kind == LUAR_IR_TNEW)
      return 1;
  return 0;
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
                                          (TValue *)base, L, &ir);
    if (!recorded) {
      partial = luar_trace_record_call_prefix(p, (int)target, (int)off,
                                              (TValue *)base, L, &ir, &callpc);
      recorded = partial;
    }
    if (recorded && ir_has_tnew(&ir) && !J->enable_alloc) {
      J->ir_aborts++;
      recorded = 0;
    }
    if (recorded) {
      J->ir_traces++;
      J->ir_ops += (uint32_t)ir.nins;
      int newcode = 0;
      {
        int intok = ir_integer_only(&ir);
        int dynfloat = intok && ir_has_float_get(p, (TValue *)base, &ir);
        int fltok = (!intok || dynfloat) ?
                    ir_floatify(p, (TValue *)base, &ir) : 0;
        /* A trace that looked integer-only but reads a float through a
        ** dynamic GETTABLE must not be emitted as integer code. */
        if (dynfloat && !fltok)
          intok = 0;
        /* jit.flush() bumped the epoch and unmapped the old trace, but a
        ** Proto still holds its pointer.  Do not treat that stale pointer
        ** as an installed trace: clear it so this recording can install a
        ** fresh one.  (luari_jit_try_run clears it too, but the hot hook
        ** runs first and consumes the one-shot seen bit.) */
        if (p->luar_ir_code != NULL && p->luar_ir_epoch != J->epoch) {
          p->luar_ir_code = NULL;
          p->luar_ir_size = 0;
          p->luar_ir_pc = -1;
          p->luar_ir_start = -1;
          p->luar_ir_base = -1;
          p->luar_ir_partial = 0;
          p->luar_ir_call_pc = -1;
        }
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
        if (partial && callpc >= 0 && callpc + 1 <= (int)off &&
            p->luar_cont_code == NULL) {
          LuaR_TraceIR cir;
          if (luar_trace_record_suffix(p, callpc + 1, (int)off,
                                       (TValue *)base, L, &cir)) {
            int cintok = ir_integer_only(&cir);
            int cfltok = (!cintok && ir_floatify(p, (TValue *)base, &cir));
            if ((cintok || cfltok) && J->ncode < LUAR_JIT_MAXCODE &&
                luari_emit_trace_ir(&cir, &p->luar_cont_code,
                                    &p->luar_cont_size)) {
              J->code[J->ncode].code = p->luar_cont_code;
              J->code[J->ncode].size = p->luar_cont_size;
              J->ncode++;
              p->luar_cont_call_pc = callpc;
              p->luar_cont_backedge = (int)off;
              p->luar_cont_epoch = J->epoch;
              J->ir_cont_compiled++;
            }
          }
        }
      }
      else
        J->ir_install_rejects++;
      if (J->dump_ir) {
        fprintf(stderr, "-- j4 ir: %d ops, %d exits, depth %d\n",
                ir.nins, ir.nexits, ir.depth);
      }
    }
    else
      J->ir_aborts++;
  }
  /* J4 only owns integer-controlled for-loop backedges.  When ir=1 is
  ** enabled but this backedge was not installed as an IR root (a while or
  ** repeat loop, a float for-loop, a rejected trace, or a stale pointer
  ** after jit.flush()), fall through to the J2 emitter instead of leaving
  ** the loop interpreted.  luari_jit_try_run still checks the IR root
  ** first, so a compiled IR trace keeps ownership. */
  if (J->enabled &&
      (!J->enable_ir ||
       p->luar_ir_code == NULL ||
       p->luar_ir_epoch != J->epoch ||
       p->luar_ir_pc != (int)off)) {
    int ok = try_compile(L, p, target, off, base);
    int full = (p->luar_code[0] != NULL || p->luar_tint[0] != 0) &&
               (p->luar_code[1] != NULL || p->luar_tint[1] != 0);
    /* LUAR: only stop compiling when both per-Proto slots are occupied.
    ** A rejected backedge is already recorded in luar_seen, so keeping the
    ** hot counter at the threshold lets an independent inner or sibling
    ** loop in the same Proto get its own attempt. */
    (void)ok;
    if (full)
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
    if (J->active_depth > 0)
      J->flush_pending = 1;   /* cannot unmap code that is still running */
    else
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
    else if (J != NULL && strncmp(opt, "alloc=", 6) == 0)
      J->enable_alloc = (opt[6] != '0');
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
      char *end = NULL;
      unsigned long v = strtoul(opt + 8, &end, 10);
      if (end != NULL && *end == '\0' && v <= 100000)
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
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->tint_hits : 0);
  lua_setfield(L, -2, "tint_hits");
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
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_entry_checks : 0);
  lua_setfield(L, -2, "ir_entry_checks");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_entry_pc_mismatch : 0);
  lua_setfield(L, -2, "ir_entry_pc_mismatch");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_entry_base_mismatch : 0);
  lua_setfield(L, -2, "ir_entry_base_mismatch");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_entry_failures : 0);
  lua_setfield(L, -2, "ir_entry_failures");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_last_entry_result : 0);
  lua_setfield(L, -2, "ir_last_entry_result");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_alloc_tables : 0);
  lua_setfield(L, -2, "ir_alloc_tables");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_compiled : 0);
  lua_setfield(L, -2, "ir_side_compiled");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_executed : 0);
  lua_setfield(L, -2, "ir_side_executed");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_call_exits : 0);
  lua_setfield(L, -2, "ir_call_exits");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_cont_compiled : 0);
  lua_setfield(L, -2, "ir_cont_compiled");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_cont_runs : 0);
  lua_setfield(L, -2, "ir_cont_runs");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_install_rejects : 0);
  lua_setfield(L, -2, "ir_install_rejects");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_start : -1);
  lua_setfield(L, -2, "ir_side_start");
  lua_pushinteger(L, J != NULL ? (lua_Integer)J->ir_side_parent : -1);
  lua_setfield(L, -2, "ir_side_parent");
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


LUAR_NO_SANITIZE_FUNCTION
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


LUAR_NO_SANITIZE_FUNCTION
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


LUAR_NO_SANITIZE_FUNCTION
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


LUAR_NO_SANITIZE_FUNCTION
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


LUAR_NO_SANITIZE_FUNCTION
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


LUAR_NO_SANITIZE_FUNCTION
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


LUAR_NO_SANITIZE_FUNCTION
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


LUAR_NO_SANITIZE_FUNCTION
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
