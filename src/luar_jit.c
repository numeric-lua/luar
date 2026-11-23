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
    if (kc == arr || kc == for_a || kc == for_a + 1 ||
        (!ttisinteger(&b[kc]) && !ttisfloat(&b[kc])))
      return 0;
    return install_cdata_tint(p, J, backedge, 20, arr, kc, 0, 0, key,
                              0, for_a);
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
  ptrdiff_t n = backedge - start + 1;
  Instruction g, opins, s;
  int key = for_a + 2;
  int tmp, src, dst, op, scalar = -1, scalar_const = -1;
  OpCode opc;
  if (n != 5 || GET_OPCODE(p->code[start]) != OP_GETTABLE)
    return 0;
  g = p->code[start];
  opins = p->code[start + 1];
  s = p->code[start + 3];
  tmp = GETARG_A(g);
  src = GETARG_B(g);
  if (GETARG_C(g) != key || tmp == key || tmp == src || src == key ||
      is_control_reg(tmp, for_a) || is_control_reg(src, for_a) ||
      luar_ffi_trace_array_kind(&b[src]) == 0)
    return 0;
  opc = GET_OPCODE(opins);
  switch (opc) {
    case OP_ADD: case OP_ADDI: case OP_ADDK: op = 0; break;
    case OP_SUB: case OP_SUBK: op = 1; break;
    case OP_MUL: case OP_MULK: op = 2; break;
    case OP_DIV: case OP_DIVK: op = 3; break;
    case OP_IDIV: case OP_IDIVK: op = 4; break;
    case OP_MOD: case OP_MODK: op = 5; break;
    default: return 0;
  }
  if (GETARG_A(opins) != tmp)
    return 0;
  if (opc == OP_ADD || opc == OP_SUB || opc == OP_MUL || opc == OP_DIV ||
      opc == OP_IDIV || opc == OP_MOD) {
    int bop = GETARG_B(opins), cop = GETARG_C(opins);
    int other;
    if (bop == tmp)
      other = cop;
    else if (cop == tmp && (opc == OP_ADD || opc == OP_MUL))
      other = bop;   /* compiler swapped a commutative form */
    else
      return 0;
    if (other == key || other == for_a || other == for_a + 1 ||
        other == for_a + 2 || other == src || other == tmp ||
        (!ttisinteger(&b[other]) && !ttisfloat(&b[other])))
      return 0;
    scalar = other;
  }
  else if (opc == OP_ADDI) {
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
  if (GET_OPCODE(p->code[start + 2]) != OP_MMBIN &&
      GET_OPCODE(p->code[start + 2]) != OP_MMBINI &&
      GET_OPCODE(p->code[start + 2]) != OP_MMBINK)
    return 0;
  if (GET_OPCODE(s) != OP_SETTABLE || GETARG_k(s) || GETARG_B(s) != key ||
      GETARG_C(s) != tmp)
    return 0;
  dst = GETARG_A(s);
  if (dst == tmp || dst == for_a || dst == for_a + 1 ||
      dst == for_a + 2 ||
      luar_ffi_trace_array_store_kind(&b[dst]) == 0)
    return 0;
  return install_cdata_tint(p, J, backedge, 24, dst, src, scalar, op, tmp,
                            scalar_const, for_a);
}


/* Match `a[i] = b[i] OP c[i]`: GETTABLE ta, b, key; GETTABLE tb, c, key;
** arithmetic; SETTABLE.  The arithmetic destination must be ta, which is
** the common compiler shape. */
static int match_cdata_pair_store (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                                   ptrdiff_t backedge, int for_a, TValue *b) {
  ptrdiff_t n = backedge - start + 1;
  Instruction g1, g2, opins, s;
  int key = for_a + 2, op;
  if (n != 6 || GET_OPCODE(p->code[start]) != OP_GETTABLE ||
      GET_OPCODE(p->code[start + 1]) != OP_GETTABLE)
    return 0;
  g1 = p->code[start];
  g2 = p->code[start + 1];
  opins = p->code[start + 2];
  s = p->code[start + 4];
  if (GETARG_C(g1) != key || GETARG_C(g2) != key)
    return 0;
  if (GET_OPCODE(p->code[start + 3]) != OP_MMBIN ||
      GET_OPCODE(s) != OP_SETTABLE || GETARG_k(s) ||
      GETARG_B(s) != key)
    return 0;
  if (GETARG_A(opins) != GETARG_A(g1) ||
      GETARG_B(opins) != GETARG_A(g1) ||
      GETARG_C(opins) != GETARG_A(g2))
    return 0;
  switch (GET_OPCODE(opins)) {
    case OP_ADD: op = 0; break;
    case OP_SUB: op = 1; break;
    case OP_MUL: op = 2; break;
    case OP_DIV: op = 3; break;
    case OP_IDIV: op = 4; break;
    case OP_MOD: op = 5; break;
    default: return 0;
  }
  {
    int dst = GETARG_A(s), av = GETARG_B(g1), bv = GETARG_B(g2);
    int ta = GETARG_A(g1), tb = GETARG_A(g2);
    if (GETARG_C(s) != ta || dst == for_a || dst == for_a + 1 ||
        dst == for_a + 2 || av == for_a || av == for_a + 1 ||
        av == for_a + 2 || bv == for_a || bv == for_a + 1 ||
        bv == for_a + 2 || ta == key || tb == key || ta == tb ||
        ta == av || ta == bv || ta == dst ||
        tb == av || tb == bv || tb == dst ||
        luar_ffi_trace_array_store_kind(&b[dst]) == 0 ||
        luar_ffi_trace_array_kind(&b[av]) == 0 ||
        luar_ffi_trace_array_kind(&b[bv]) == 0)
      return 0;
    return install_cdata_tint(p, J, backedge, 23, dst, av, bv, ta, tb, op,
                              for_a);
  }
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
  p->luar_tint_pc[slot] = (int)backedge;
  p->luar_tint_table[slot] = encode_pair_source(r1);
  p->luar_tint_keys[slot] = encode_pair_source(r2);
  p->luar_tint_upval[slot] = encode_pair_source(r3);
  p->luar_tint_acc[slot] = acc;
  p->luar_tint_res[slot] = res;
  p->luar_tint_base[slot] = for_a;
  p->luar_tint_epoch[slot] = J->epoch;
  J->compiled++;
  return 1;
}


/* LUAR: `s = s + a[i] * k` where `k` is a numeric constant, a register
** scalar or a closure upvalue scalar.  The scalar encoding is positive for
** a register, -1-index for an upvalue and 0x40000000|k for a Proto
** constant. */
#define LUAR_TINT_SCALAR_CONST 0x40000000

static int install_scaled_sum (Proto *p, LuaR_JIT *J, ptrdiff_t backedge,
                               int for_a, const LuaR_PairRead *r,
                               int scalar_enc, int elem_reg, int prod_reg,
                               int acc) {
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
  p->luar_tint[slot] = 15;
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
  if (GET_OPCODE(mul) == OP_MULK) {
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
  else if (GET_OPCODE(mul) == OP_MUL) {
    mul_a = GETARG_A(mul);
    mul_b = GETARG_B(mul);
    mul_c = GETARG_C(mul);
    if (mul_b == r.val)
      scalar_reg = mul_c;
    else if (mul_c == r.val)
      scalar_reg = mul_b;
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
  if (GET_OPCODE(add) != OP_ADD)
    return 0;
  acc = GETARG_A(add);
  if (!((GETARG_B(add) == acc && GETARG_C(add) == mul_a) ||
        (GETARG_B(add) == mul_a && GETARG_C(add) == acc)))
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
  if (scalar_enc >= 0 && scalar_enc < LUAR_TINT_SCALAR_CONST) {
    /* A register scalar must not be overwritten by the element read or
    ** the multiply; otherwise later iterations see a different value. */
    if (scalar_reg == r.val || scalar_reg == prod_reg || scalar_reg == acc)
      return 0;
    if (!ttisinteger(&b[scalar_reg]) && !ttisfloat(&b[scalar_reg]))
      return 0;
  }
  if (r.upval < 0 && !ttistable(&b[r.table]))
    return 0;
  if (!ttisinteger(&b[acc]) && !ttisfloat(&b[acc]))
    return 0;
  return install_scaled_sum(p, J, backedge, for_a, &r, scalar_enc,
                            r.val, prod_reg, acc);
}


/* LUAR: `s = s + a[i] * b[i]` (and its `a[i] * a[i]` square special case).
** The two reads and the MUL live in a temporary, then the accumulator gets
** the product.  Both sources may be registers or upvalues. */
static int match_dot_product (Proto *p, LuaR_JIT *J, ptrdiff_t start,
                              ptrdiff_t backedge, int for_a, TValue *base) {
  LuaR_PairRead ra, rb;
  Instruction mul, mm1, add, mm2;
  ptrdiff_t i = start;
  int res, acc, mb, mc, ab, ac;
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
  if (GET_OPCODE(mul) != OP_MUL)
    return 0;
  res = GETARG_A(mul);
  mb = GETARG_B(mul);
  mc = GETARG_C(mul);
  if (!((mb == ra.val && mc == rb.val) ||
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
  if (GET_OPCODE(add) != OP_ADD)
    return 0;
  acc = GETARG_A(add);
  ab = GETARG_B(add);
  ac = GETARG_C(add);
  if (!((ab == acc && ac == res) || (ab == res && ac == acc)))
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
    return install_pair_intrinsic(p, J, backedge, for_a, &ra, &rb, acc, res,
                                  (acd != 0) ? 19 : 14);
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
    if (p->luar_tint[i] != 0 && p->luar_tint_epoch[i] != J->epoch)
      p->luar_tint[i] = 0;
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
      if (GETARG_C(i0) == for_a + 2 &&
          GETARG_B(i1) == GETARG_A(i0) &&
          GETARG_C(i1) == for_a + 2)
        return install_keyed_fill(p, J, backedge, for_a,
                                  GETARG_A(i1), GETARG_B(i0), -1);
    }
    if (!for_float && J->enable_tint && n >= 4 && GET_OPCODE(i0) == OP_GETUPVAL &&
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
    if (!for_float && J->enable_tint && n >= 5 && GET_OPCODE(i0) == OP_GETTABLE &&
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
                                 GETARG_A(i2), GETARG_A(i1));
    }
    if (!for_float && J->enable_tint && n >= 5 &&
        GET_OPCODE(i0) == OP_GETUPVAL &&
        GET_OPCODE(p->code[start + 1]) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 2]) == OP_ADD &&
        GET_OPCODE(p->code[start + 3]) == OP_MMBIN &&
        backedge == start + 4) {
      /* LUAR: `s = s + a[i]` where `a` is a bounded cdata array captured
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
          av != NULL && luar_ffi_trace_array_kind(av) != 0)
        return install_cdata_tint(p, J, backedge, 16, -1 - GETARG_B(i0),
                                  0, 0, acc, tmp, 0, for_a);
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
          treg != for_a && treg != for_a + 1 && treg != for_a + 2)
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
      if (GETARG_A(i0) == GETARG_B(i1) &&
          GETARG_C(i1) == for_a + 2 &&
          GETARG_C(i2) == GETARG_A(i1) &&
          GETARG_A(i2) == GETARG_A(i1) &&
          GETARG_A(i3) == GETARG_B(i3) &&
          GETARG_C(i3) == GETARG_A(i2))
        return install_keyed_sum(p, J, backedge, for_a,
                                 GETARG_B(i2), 0, GETARG_B(i0),
                                 GETARG_A(i3), GETARG_A(i2));
    }
    if (!for_float && J->enable_tint && n >= 4 &&
        GET_OPCODE(i0) == OP_GETTABLE &&
        GET_OPCODE(p->code[start + 1]) == OP_ADD &&
        GET_OPCODE(p->code[start + 2]) == OP_MMBIN &&
        backedge == start + 3) {
      /* LUAR: `s = s + a[i]` over a bounded FFI cdata array.  The generic
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
          luar_ffi_trace_array_kind(&b[arr]) != 0)
        return install_cdata_tint(p, J, backedge, 16, arr, 0, 0,
                                  acc, tmp, 0, for_a);
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
          if (GET_OPCODE(nx) == OP_ADD) {
            int na = GETARG_A(nx), nb = GETARG_B(nx), nc = GETARG_C(nx);
            int acct = 0;
            if ((nb == ka && nc == na) || (nc == ka && nb == na))
              acct = slot_type(b, cur, def, na);
            if ((acct == LUAR_T_INT || acct == LUAR_T_FLOAT) &&
                !is_control_reg(na, for_a) && ka != na) {
              /* R[ka] is part of the ADD's bytecode effect; materialise it
              ** so later reads (and the post-loop value) stay correct. */
              op->kind = LUAR_OP_TABLE_ADD;
              op->a = (uint8_t)na;
              op->b = (uint8_t)kb;
              op->c = (uint8_t)kc;
              op->res = (uint8_t)ka;
              op->ta = (uint8_t)acct;
              op->tb = LUAR_T_TABLE;
              op->tc = LUAR_T_INT;
              op->elem = LUAR_T_INT;
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
    }
  }

  /* Fast fill: t[idx] = idx with the loop index as both key and value. */
  if (!for_float && !is_while && d.nops == 1 &&
      ops[0].kind == LUAR_OP_SETTABLE &&
      ops[0].b == for_a + 2 && ops[0].c == for_a + 2) {
    d.fast_fill = 1;
    d.ff_table = ops[0].a;
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
    p->luar_tint_pc[slot] = (int)backedge;
    p->luar_tint_table[slot] = d.fast_fill ? d.ff_table : d.ft_table;
    p->luar_tint_acc[slot] = d.fast_tsum ? d.ft_acc : 0;
    p->luar_tint_res[slot] = d.fast_tsum ? d.ft_res : 0;
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
                            int acc_reg, int res_reg) {
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
        if (promoted)
          fsum += cast_num(getArrVal(h, key)->i);
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
        fsum += getArrVal(h, key)->n;
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
                                int res_reg) {
  TValue *tt = &base[table_reg];
  if (!ttistable(tt))
    return 0;
  return table_sum_table(L, base, for_a, hvalue(tt), acc_reg, res_reg);
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


static Table *pair_source_table (TValue *base, int reg, int upval);


static int table_sum3_intrinsic (lua_State *L, TValue *base, int for_a,
                               int s1, int s2, int s3, int acc_reg,
                               int res_reg) {
  TValue *ct = &base[for_a];
  TValue *st = &base[for_a + 1];
  TValue *it = &base[for_a + 2];
  TValue *at = &base[acc_reg];
  Table *h[3];
  int enc[3];
  lua_Integer count, step, idx, last, ckey, k;
  int j;
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
  if (ttisinteger(at)) {
    lua_Unsigned sum = l_castS2U(ivalue(at));
    lua_Number fsum = 0;
    int promoted = 0;
    for (k = 0, ckey = idx - 1; k <= count; k++, ckey += step) {
      unsigned key = (unsigned)ckey;
      for (j = 0; j < 3; j++) {
        lu_byte tag = *getArrTag(h[j], key);
        if (tag == LUA_VNUMINT) {
          if (promoted)
            fsum += cast_num(getArrVal(h[j], key)->i);
          else
            sum += l_castS2U(getArrVal(h[j], key)->i);
        }
        else if (tag == LUA_VNUMFLT) {
          if (!promoted) {
            fsum = cast_num(l_castU2S(sum));
            promoted = 1;
          }
          fsum += getArrVal(h[j], key)->n;
        }
        else
          return 0;
      }
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
      for (j = 0; j < 3; j++) {
        lu_byte tag = *getArrTag(h[j], key);
        if (tag == LUA_VNUMINT)
          sum += cast_num(getArrVal(h[j], key)->i);
        else if (tag == LUA_VNUMFLT)
          sum += getArrVal(h[j], key)->n;
        else
          return 0;
      }
    }
    setfltvalue(at, sum);
  }
  else
    return 0;
  if (res_reg != acc_reg && res_reg >= 0) {
    unsigned rkey = (unsigned)(last - 1);
    lu_byte rtag = *getArrTag(h[2], rkey);
    if (rtag == LUA_VNUMINT) {
      setivalue(&base[res_reg], getArrVal(h[2], rkey)->i);
    }
    else {
      setfltvalue(&base[res_reg], getArrVal(h[2], rkey)->n);
    }
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


static Table *pair_source_table (TValue *base, int reg, int upval) {
  TValue *tv = pair_source_value(base, reg, upval);
  return (tv != NULL && ttistable(tv)) ? hvalue(tv) : NULL;
}


/* kind 12: s = s + a[i] + b[i]
** kind 13: s = s + a[i] - b[i]
** kind 14: s = s + a[i] * b[i]  (a and b may be the same table) */
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
    if (kind == 14) {
      LuaR_PairNum prod = pair_mul(av, bv);
      last_temp = prod;
      acc = pair_add(acc, prod);
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


/* kind 15: s = s + a[i] * k.  The scalar is numeric in every iteration;
** the element and product registers keep their last bytecode values. */
static int scaled_array_intrinsic (lua_State *L, TValue *base, int for_a,
                                   Table *h, const TValue *scalar,
                                   int elem_reg, int prod_reg, int acc_reg) {
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
    last_prod = pair_mul(elem, fac);
    acc = pair_add(acc, last_prod);
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
  int r;
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
      p->luar_ir_partial = 0;
      p->luar_ir_call_pc = -1;
    }
    else if (p->luar_ir_pc == pc && p->luar_ir_base == expected_base) {
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
        p->luar_tint_base[i] == expected_base) {
      int r;
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
                                  p->luar_tint_res[i]);
          break;
        case 3: {
          Table *keys = tint_keys_table(p, (TValue *)base, i);
          r = (keys == NULL) ? 0
              : keyed_fill_intrinsic(L, (TValue *)base, expected_base,
                                     p->luar_tint_table[i], keys);
          break;
        }
        case 10: {
          Table *h = tint_upval_table(p, (TValue *)base, i);
          r = (h == NULL) ? 0
              : table_sum_table(L, (TValue *)base, expected_base, h,
                                p->luar_tint_acc[i], p->luar_tint_res[i]);
          break;
        }
        case 11:
          r = table_sum3_intrinsic(L, (TValue *)base, expected_base,
                                   p->luar_tint_table[i],
                                   p->luar_tint_keys[i],
                                   p->luar_tint_upval[i],
                                   p->luar_tint_acc[i],
                                   p->luar_tint_res[i]);
          break;
        case 12:
        case 13:
        case 14: {
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
        case 15: {
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
                                       p->luar_tint_acc[i]);
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
                    &((TValue *)base)[p->luar_tint_res[i]]);
          break;
        }
        case 17:
        case 18:
        case 19: {
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
                    kind - 17);
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
        case 21:
          r = luar_ffi_trace_copy_array(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[p->luar_tint_keys[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_acc[i]]);
          break;
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
                                  hvalue(&field), p->luar_tint_acc[i], tmp);
          }
          break;
        }
        case 23:
          r = luar_ffi_trace_pair_store(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[p->luar_tint_keys[i]],
                  &((TValue *)base)[p->luar_tint_upval[i]],
                  &((TValue *)base)[expected_base + 2],
                  &((TValue *)base)[expected_base],
                  &((TValue *)base)[expected_base + 1],
                  &((TValue *)base)[p->luar_tint_acc[i]],
                  &((TValue *)base)[p->luar_tint_res[i]],
                  p->luar_tint_arg4[i]);
          break;
        case 24: {
          TValue scalar;
          int enc = p->luar_tint_upval[i];
          if (enc == -2) {
            setivalue(&scalar, (lua_Integer)p->luar_tint_arg4[i]);
          }
          else if (enc == -1)
            scalar = p->k[p->luar_tint_arg4[i]];
          else
            scalar = ((TValue *)base)[enc];
          r = luar_ffi_trace_scalar_store(
                  &((TValue *)base)[p->luar_tint_table[i]],
                  &((TValue *)base)[p->luar_tint_keys[i]],
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
        default: {
          Table *keys = tint_keys_table(p, (TValue *)base, i);
          r = (keys == NULL) ? 0
              : keyed_sum_intrinsic(L, (TValue *)base, expected_base,
                                    p->luar_tint_table[i], keys,
                                    p->luar_tint_acc[i],
                                    p->luar_tint_res[i]);
          break;
        }
      }
      if (r == 0) {
        J->exits++;
        /* LUAR: a raw-table intrinsic that cannot cover this iteration
        ** (non-array shape, metatable, short table, size cap) would fail
        ** again at every backedge.  A missing key late in a large loop
        ** made that quadratic.  The backedge is already in the seen bloom,
        ** so dropping the slot falls back to the interpreter for good. */
        if (kind <= 4 || (kind >= 10 && kind <= 26))
          p->luar_tint[i] = 0;
      }
      else
        J->tint_hits++;   /* LUAR: observable proof the intrinsic ran */
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

/* FFI traces have their own cdata/call type rules and were already covered
** by the FFI J4 suite; leave their floatification alone. */
static int ir_has_ffi_ops (const LuaR_TraceIR *ir) {
  int i;
  for (i = 0; i < ir->nins; i++) {
    switch (ir->ins[i].kind) {
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
      case LUAR_IR_CCPYI64:
      case LUAR_IR_CADDI64:
      case LUAR_IR_CMULI64:
      case LUAR_IR_CDIVI64:
      case LUAR_IR_CUDIVI64:
      case LUAR_IR_CMODI64:
      case LUAR_IR_CUMODI64:
      case LUAR_IR_CBANDI64:
      case LUAR_IR_CBORI64:
      case LUAR_IR_CBXORI64:
      case LUAR_IR_CSHLI64:
      case LUAR_IR_CSHRI64:
      case LUAR_IR_CNEGI64:
      case LUAR_IR_CBNOTI64:
      case LUAR_IR_CSETI64:
      case LUAR_IR_CCALL0I:
      case LUAR_IR_CCALL0D:
      case LUAR_IR_CCALL1I:
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
      case LUAR_IR_CCALL2II:
      case LUAR_IR_CCALL3DD:
      case LUAR_IR_CCALL3DDI:
      case LUAR_IR_CCALL3DID:
      case LUAR_IR_CCALL3IDD:
        return 1;
      default:
        break;
    }
  }
  return 0;
}


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
        type[in->a] = (in->b < ns) ? type[in->b] : 0;
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
        if (type[in->b] != 1 || in->nval != (double)in->ival) return 0;
        type[in->a] = 1;
        break;
      case LUAR_IR_MULK:
        if (type[in->b] != 1 || in->nval != (double)in->ival) return 0;
        type[in->a] = 1;
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
        if (type[in->b] != 2) return 0;
        type[in->a] = 2;
        break;
      case LUAR_IR_FADDI:
        if (type[in->b] != 2) return 0;
        type[in->a] = 2;
        break;
      case LUAR_IR_FADD:
      case LUAR_IR_FSUB:
        if (type[in->b] != 2 || type[in->c] != 2) return 0;
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
      default:
        break;
    }
  }
  return 1;
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
      sawtable = 1;
      continue;
    }
    if (op->kind == LUAR_IR_ADDK || op->kind == LUAR_IR_SUBK ||
        op->kind == LUAR_IR_MODK || op->kind == LUAR_IR_IDIVK) {
      /* LUAR: there is no float form of these K ops.  A float constant is
      ** still recorded as an integer kind with ival == 0; emitting it would
      ** compute with 0 and retag the result as an integer.  Reject so the
      ** side trace falls back to the interpreter instead of miscompiling. */
      if (op->nval != (double)op->ival)
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
  return sawtable && sawfloat &&
         (ir_has_ffi_ops(ir) || ir_float_consistent(ir, base));
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
  return 1;
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
    else if (in->kind == LUAR_IR_TGET)
      tag[in->a] = -1;   /* dynamic value; following guards decide */
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
  if (J->enabled && !J->enable_ir) {
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
