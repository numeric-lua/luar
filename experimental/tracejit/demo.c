/*
** Luar trace-JIT toolchain proof-of-concept.
**
** Loads a fixed Lua 5.5 chunk, reads the Proto from the loaded LClosure,
** prints/disassembles its bytecode, decodes the numeric-for loop, and
** asks DynASM to assemble an x86-64 loop from the decoded parameters.
** The interpreted and native results are compared for several n.
**
** There is no guard, deopt, GC safepoint, or VM/frame integration here.
** See README.md for the precise scope of what is and is not proven.
*/

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "lua.h"
#include "lauxlib.h"
#include "lobject.h"
#include "lstate.h"
#include "lopcodes.h"
#include "lopnames.h"
#include "trace.h"

static const char *CHUNK =
  "local s = 0; for i = 1, n do s = s + i end; return s";

/* ---------------------------------------------------------------- decode */

typedef enum {
  DEF_NONE = 0,
  DEF_LOADI,
  DEF_LOADK,
  DEF_GETTABUP
} DefKind;

typedef struct {
  DefKind kind;
  int pc;
  int64_t ival;   /* LOADI */
  int kidx;       /* LOADK / GETTABUP constant index */
  int uvidx;      /* GETTABUP upvalue index */
} Def;

static const char *def_name(DefKind k)
{
  switch (k) {
    case DEF_LOADI: return "LOADI";
    case DEF_LOADK: return "LOADK";
    case DEF_GETTABUP: return "GETTABUP";
    default: return "none";
  }
}

/* Find the last instruction before 'before' that defines register 'reg'. */
static int find_def(const Proto *p, int reg, int before, Def *def)
{
  int pc;
  memset(def, 0, sizeof(*def));
  def->kind = DEF_NONE;
  for (pc = before - 1; pc >= 0; pc--) {
    Instruction ins = p->code[pc];
    OpCode op = GET_OPCODE(ins);
    if (!testAMode(op) || GETARG_A(ins) != reg)
      continue;
    switch (op) {
      case OP_LOADI:
        def->kind = DEF_LOADI;
        def->pc = pc;
        def->ival = GETARG_sBx(ins);
        return 1;
      case OP_LOADK:
        def->kind = DEF_LOADK;
        def->pc = pc;
        def->kidx = GETARG_Bx(ins);
        return 1;
      case OP_GETTABUP:
        def->kind = DEF_GETTABUP;
        def->pc = pc;
        def->uvidx = GETARG_B(ins);
        def->kidx = GETARG_C(ins);
        return 1;
      default:
        break;
    }
  }
  return 0;
}

static int def_to_int(const Proto *p, const Def *def, int64_t *out)
{
  if (def->kind == DEF_LOADI) {
    *out = def->ival;
    return 1;
  }
  if (def->kind == DEF_LOADK && def->kidx >= 0 && def->kidx < p->sizek) {
    const TValue *k = &p->k[def->kidx];
    if (ttisinteger(k)) {
      *out = ivalue(k);
      return 1;
    }
  }
  return 0;
}

static void print_instruction(const Proto *p, int pc)
{
  Instruction ins = p->code[pc];
  OpCode op = GET_OPCODE(ins);

  printf("  pc %2d  %-13s", pc, opnames[op]);
  switch (getOpMode(op)) {
    case iABC:
      printf("A=%d B=%d C=%d k=%d", GETARG_A(ins), GETARG_B(ins),
             GETARG_C(ins), GETARG_k(ins));
      break;
    case ivABC:
      printf("A=%d vB=%d vC=%d k=%d", GETARG_A(ins), GETARG_vB(ins),
             GETARG_vC(ins), GETARG_k(ins));
      break;
    case iABx:
      printf("A=%d Bx=%d", GETARG_A(ins), GETARG_Bx(ins));
      break;
    case iAsBx:
      printf("A=%d sBx=%d", GETARG_A(ins), GETARG_sBx(ins));
      break;
    case iAx:
      printf("Ax=%d", GETARG_Ax(ins));
      break;
    case isJ:
      printf("sJ=%d", GETARG_sJ(ins));
      break;
  }

  switch (op) {
    case OP_GETTABUP: {
      int c = GETARG_C(ins);
      if (c >= 0 && c < p->sizek) {
        const TValue *k = &p->k[c];
        if (ttisstring(k))
          printf("  ; K[%d]=\"%s\"", c, getstr(tsvalue(k)));
      }
      break;
    }
    case OP_ADD:
      printf("  ; R%d := R%d + R%d", GETARG_A(ins), GETARG_B(ins),
             GETARG_C(ins));
      break;
    case OP_ADDI:
      printf("  ; R%d := R%d + %d", GETARG_A(ins), GETARG_B(ins),
             GETARG_sC(ins));
      break;
    case OP_MMBIN:
    case OP_MMBINI:
    case OP_MMBINK:
      printf("  ; metamethod fallback (integer fast path skips this)");
      break;
    case OP_FORPREP:
      printf("  ; prepare R%d..R%d, skip to pc %d if no iterations",
             GETARG_A(ins), GETARG_A(ins) + 2,
             pc + GETARG_Bx(ins) + 2);
      break;
    case OP_FORLOOP:
      printf("  ; if more, R%d += R%d and jump to pc %d",
             GETARG_A(ins) + 2, GETARG_A(ins) + 1, pc - GETARG_Bx(ins) + 1);
      break;
    default:
      break;
  }
  printf("\n");
}

static void print_proto(const Proto *p)
{
  int i;
  printf("=== Proto from LClosure->p ===\n");
  printf("  params=%d maxstack=%d code=%d constants=%d nested protos=%d\n",
         p->numparams, p->maxstacksize, p->sizecode, p->sizek, p->sizep);
  printf("  constants:");
  for (i = 0; i < p->sizek; i++) {
    const TValue *k = &p->k[i];
    printf(" K[%d]=", i);
    if (ttisnil(k)) printf("nil");
    else if (ttisboolean(k)) printf("%s", ttistrue(k) ? "true" : "false");
    else if (ttisinteger(k)) printf("%lld", (long long)ivalue(k));
    else if (ttisfloat(k)) printf("%g", (double)fltvalue(k));
    else if (ttisstring(k)) printf("\"%s\"", getstr(tsvalue(k)));
    else printf("<value>");
  }
  printf("\n\n=== Bytecode walk (p->code) ===\n");
  for (i = 0; i < p->sizecode; i++)
    print_instruction(p, i);
  printf("\n");
}

/* Decode the one supported loop shape from the Proto's bytecode. */
static int decode_trace(const Proto *p, TraceDesc *d)
{
  int pc, fp = -1, fl = -1, arith = -1, mm = -1;
  int a, aa, bb, cc;
  Def di, dl, ds, da;

  memset(d, 0, sizeof(*d));

  for (pc = 0; pc < p->sizecode; pc++) {
    OpCode op = GET_OPCODE(p->code[pc]);
    if (op == OP_FORPREP) {
      if (fp >= 0) { fprintf(stderr, "decode: multiple FORPREP\n"); return 0; }
      fp = pc;
    } else if (op == OP_FORLOOP) {
      if (fl >= 0) { fprintf(stderr, "decode: multiple FORLOOP\n"); return 0; }
      fl = pc;
    }
  }
  if (fp < 0 || fl < 0) {
    fprintf(stderr, "decode: no FORPREP/FORLOOP pair\n");
    return 0;
  }
  /* Exactly one arithmetic op and its following MM instruction. */
  if (fl != fp + 3) {
    fprintf(stderr, "decode: unsupported loop body (FORPREP at %d, "
                    "FORLOOP at %d)\n", fp, fl);
    return 0;
  }
  for (pc = fp + 1; pc < fl; pc++) {
    OpCode op = GET_OPCODE(p->code[pc]);
    if (op == OP_ADD || op == OP_ADDI) {
      if (arith >= 0) {
        fprintf(stderr, "decode: multiple arithmetic ops in body\n");
        return 0;
      }
      arith = pc;
    } else if (op == OP_MMBIN || op == OP_MMBINI || op == OP_MMBINK) {
      mm = pc;
    } else {
      fprintf(stderr, "decode: unsupported body opcode %s at pc %d\n",
              opnames[op], pc);
      return 0;
    }
  }
  if (arith < 0 || mm != arith + 1) {
    fprintf(stderr, "decode: expected exactly one ADD/ADDI followed by MMBIN\n");
    return 0;
  }

  a = GETARG_A(p->code[fp]);
  if (GETARG_A(p->code[fl]) != a) {
    fprintf(stderr, "decode: FORPREP/FORLOOP A mismatch\n");
    return 0;
  }
  if (a < 0 || a + 3 > p->maxstacksize) {
    fprintf(stderr, "decode: loop registers exceed maxstacksize\n");
    return 0;
  }
  d->for_reg = a;
  d->init_reg = a;
  d->limit_reg = a + 1;
  d->step_reg = a + 2;
  d->var_reg = a + 2;  /* Lua 5.4/5.5: R[A+2] is the visible loop variable */

  /* Verify the jumps form this exact loop (see FORPREP/FORLOOP in lvm.c). */
  if (fp + (int)GETARG_Bx(p->code[fp]) + 2 != fl + 1) {
    fprintf(stderr, "decode: FORPREP does not skip past FORLOOP\n");
    return 0;
  }
  if (fl - (int)GETARG_Bx(p->code[fl]) + 1 != fp + 1) {
    fprintf(stderr, "decode: FORLOOP does not jump to loop body\n");
    return 0;
  }

  /* Decode the arithmetic operation. */
  {
    Instruction ins = p->code[arith];
    OpCode op = GET_OPCODE(ins);
    aa = GETARG_A(ins);
    bb = GETARG_B(ins);
    if (op == OP_ADD) {
      cc = GETARG_C(ins);
      if (aa == d->var_reg || aa == d->init_reg ||
          aa == d->limit_reg || aa == d->step_reg) {
        fprintf(stderr, "decode: accumulator aliases a loop-control slot\n");
        return 0;
      }
      if ((bb == aa && cc == d->var_reg) ||
          (bb == d->var_reg && cc == aa)) {
        d->op = TRACE_ADD_REG;
        d->acc_reg = aa;
      } else {
        fprintf(stderr, "decode: ADD is not acc := acc + loop-variable\n");
        return 0;
      }
    } else {  /* OP_ADDI */
      if (aa != bb || aa == d->var_reg || aa == d->init_reg ||
          aa == d->limit_reg || aa == d->step_reg) {
        fprintf(stderr, "decode: ADDI is not acc := acc + immediate\n");
        return 0;
      }
      d->op = TRACE_ADD_IMM;
      d->acc_reg = aa;
      d->add_imm = GETARG_sC(ins);
    }
    if (GETARG_A(p->code[mm]) != d->acc_reg ||
        !testMMMode(GET_OPCODE(p->code[mm]))) {
      fprintf(stderr, "decode: MM instruction does not match accumulator\n");
      return 0;
    }
  }

  /* Initial value, limit and step definitions before FORPREP. */
  if (!find_def(p, a, fp, &di) || !def_to_int(p, &di, &d->init)) {
    fprintf(stderr, "decode: no integer initial value for R%d\n", a);
    return 0;
  }
  if (!find_def(p, a + 1, fp, &dl) || dl.kind != DEF_GETTABUP) {
    fprintf(stderr, "decode: limit R%d is not a GETTABUP global\n", a + 1);
    return 0;
  }
  if (dl.kidx < 0 || dl.kidx >= p->sizek || !ttisstring(&p->k[dl.kidx])) {
    fprintf(stderr, "decode: limit global name is not a string\n");
    return 0;
  }
  d->limit_name = getstr(tsvalue(&p->k[dl.kidx]));
  if (strcmp(d->limit_name, "n") != 0) {
    fprintf(stderr, "decode: limit global is \"%s\", expected \"n\"\n",
            d->limit_name);
    return 0;
  }
  if (!find_def(p, a + 2, fp, &ds) || !def_to_int(p, &ds, &d->step)) {
    fprintf(stderr, "decode: no integer step for R%d\n", a + 2);
    return 0;
  }
  if (d->step == 0) {
    fprintf(stderr, "decode: zero step\n");
    return 0;
  }
  if (!find_def(p, d->acc_reg, fp, &da) ||
      !def_to_int(p, &da, &d->acc_init)) {
    fprintf(stderr, "decode: no integer initial value for accumulator R%d\n",
            d->acc_reg);
    return 0;
  }

  printf("=== Decoded numeric-for trace ===\n");
  printf("  FORPREP pc=%d FORLOOP pc=%d (A=%d)\n", fp, fl, a);
  printf("  init  R%d = %lld   (%s @ pc %d)\n", a,
         (long long)d->init, def_name(di.kind), di.pc);
  printf("  limit R%d = _ENV[\"%s\"]   (%s @ pc %d; passed as native rdi)\n",
         a + 1, d->limit_name, def_name(dl.kind), dl.pc);
  printf("  step  R%d = %lld   (%s @ pc %d; R%d is also loop var i)\n",
         a + 2, (long long)d->step, def_name(ds.kind), ds.pc, a + 2);
  printf("  acc   R%d = %lld   (%s @ pc %d)\n", d->acc_reg,
         (long long)d->acc_init, def_name(da.kind), da.pc);
  if (d->op == TRACE_ADD_REG)
    printf("  body  R%d := R%d + R%d   (ADD @ pc %d; MMBIN @ pc %d skipped "
           "by integer fast path)\n",
           d->acc_reg, d->acc_reg, d->var_reg, arith, mm);
  else
    printf("  body  R%d := R%d + %lld   (ADDI @ pc %d; MMBIN* @ pc %d)\n",
           d->acc_reg, d->acc_reg, (long long)d->add_imm, arith, mm);
  printf("  native shape: int64_t trace_sum(int64_t n) { int64_t r%d = %lld; "
         "for (int64_t r%d = %lld; r%d %s n; r%d += %lld) r%d += ",
         d->acc_reg, (long long)d->acc_init, d->var_reg,
         (long long)d->init, d->var_reg, d->step > 0 ? "<=" : ">=",
         d->var_reg, (long long)d->step, d->acc_reg);
  if (d->op == TRACE_ADD_REG)
    printf("r%d; return r%d; }\n\n", d->var_reg, d->acc_reg);
  else
    printf("%lld; return r%d; }\n\n", (long long)d->add_imm, d->acc_reg);

  return 1;
}

/* ------------------------------------------------------------------ main */

int main(void)
{
  static const int64_t ns[] = {0, 1, 2, 10, 100, 1000, 1000000};
  const size_t ncases = sizeof(ns) / sizeof(ns[0]);
  lua_State *L = NULL;
  Proto *p;
  TraceDesc desc;
  trace_sum_fn trace_sum = NULL;
  size_t code_size = 0;
  int ref = LUA_NOREF;
  int status;
  int mismatches = 0;
  size_t i;

  printf("=== Lua 5.5 chunk ===\n%s\n\n", CHUNK);

  L = luaL_newstate();
  if (L == NULL) {
    fprintf(stderr, "demo: luaL_newstate failed\n");
    return 1;
  }
  status = luaL_loadstring(L, CHUNK);
  if (status != LUA_OK) {
    fprintf(stderr, "demo: load failed: %s\n", lua_tostring(L, -1));
    goto fail;
  }
  if (!ttisLclosure(s2v(L->top.p - 1))) {
    fprintf(stderr, "demo: loaded chunk is not an LClosure\n");
    goto fail;
  }
  p = clLvalue(s2v(L->top.p - 1))->p;
  if (p == NULL) {
    fprintf(stderr, "demo: LClosure->p is NULL\n");
    goto fail;
  }

  print_proto(p);
  if (!decode_trace(p, &desc))
    goto fail;

  /* Keep the loaded closure in the registry for repeated interpreted calls. */
  ref = luaL_ref(L, LUA_REGISTRYINDEX);

  trace_sum = trace_build(&desc, &code_size);
  if (trace_sum == NULL)
    goto fail;
  printf("=== DynASM x64 native trace ===\n");
  printf("  emitted %zu bytes at %p\n", code_size, (void *)trace_sum);
  printf("  ABI: int64_t trace_sum(int64_t n), n in rdi, result in rax\n\n");

  printf("=== interpreted (lua_pcall) vs native (DynASM) ===\n");
  printf("  %10s  %20s  %20s  %s\n", "n", "interpreted", "native", "result");
  for (i = 0; i < ncases; i++) {
    int64_t n = ns[i];
    int64_t native, interp;
    int isint;
    lua_Integer li;

    lua_pushinteger(L, (lua_Integer)n);
    lua_setglobal(L, "n");
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    status = lua_pcall(L, 0, 1, 0);
    if (status != LUA_OK) {
      fprintf(stderr, "demo: interpreted call failed for n=%lld: %s\n",
              (long long)n, lua_tostring(L, -1));
      lua_pop(L, 1);
      goto fail;
    }
    if (!lua_isinteger(L, -1)) {
      fprintf(stderr, "demo: interpreted result for n=%lld is not integer\n",
              (long long)n);
      lua_pop(L, 1);
      goto fail;
    }
    isint = 0;
    li = lua_tointegerx(L, -1, &isint);
    if (!isint) {
      fprintf(stderr, "demo: lua_tointegerx failed for n=%lld\n",
              (long long)n);
      lua_pop(L, 1);
      goto fail;
    }
    interp = (int64_t)li;
    lua_pop(L, 1);

    native = trace_sum(n);
    printf("  %10lld  %20lld  %20lld  %s\n", (long long)n,
           (long long)interp, (long long)native,
           interp == native ? "ok" : "MISMATCH");
    if (interp != native)
      mismatches++;
  }

  printf("\n%s: %zu/%zu cases match\n",
         mismatches == 0 ? "PASS" : "FAIL",
         ncases - (size_t)mismatches, ncases);

  luaL_unref(L, LUA_REGISTRYINDEX, ref);
  ref = LUA_NOREF;
  lua_close(L);
  munmap((void *)trace_sum, code_size);
  return mismatches == 0 ? 0 : 1;

fail:
  if (ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ref);
  lua_close(L);
  if (trace_sum != NULL) munmap((void *)trace_sum, code_size);
  return 1;
}
