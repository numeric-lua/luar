/*
** luar_jit.c -- the 'jit' library plus the J1 trace recorder.
**
** J1 records hot loop backedges and snapshots the loop bytecode (see
** doc/jit-design.md sections 7 and 10).  There is still no machine code:
** the interpreter executes everything and jit.status() reports false.
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
  return r == for_a || r == for_a + 1 || r == for_a + 2;
}


/* Validate and compile the loop [start, backedge].  The trace must end in
** FORLOOP and contain only integer register arithmetic.  Returns 1 when a
** loop was compiled (or a previous one is already installed). */
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
  int for_a;
  if (!luar_emit_available() || n < 2 || n > LUAR_JIT_MAXINS)
    return 0;
  last = p->code[backedge];
  if (GET_OPCODE(last) != OP_FORLOOP)
    return 0;
  for_a = GETARG_A(last);
  if (base != NULL) {
    TValue *b = (TValue *)base;
    if (!ttisinteger(&b[for_a]) || !ttisinteger(&b[for_a + 1]) ||
        !ttisinteger(&b[for_a + 2]))
      return 0;   /* float or non-integer loop: no native code */
  }
  d.ops = ops;
  d.nops = (int)n - 1;
  d.for_a = for_a;
  d.tag_int = LUA_VNUMINT;
  for (i = 0; i < n - 1; i++) {
    Instruction ins = p->code[start + i];
    LuaR_Op *op = &ops[i];
    memset(op, 0, sizeof(*op));
    switch (GET_OPCODE(ins)) {
      case OP_MOVE:
        op->kind = LUAR_OP_MOVE;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        break;
      case OP_LOADI:
        op->kind = LUAR_OP_LOADI;
        op->a = (uint8_t)GETARG_A(ins);
        op->imm = (int32_t)GETARG_sBx(ins);
        break;
      case OP_ADDI:
        op->kind = LUAR_OP_ADDI;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        op->imm = (int32_t)GETARG_sC(ins);
        break;
      case OP_ADD:
        op->kind = LUAR_OP_ADD;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        op->c = (uint8_t)GETARG_C(ins);
        break;
      case OP_SUB:
        op->kind = LUAR_OP_SUB;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        op->c = (uint8_t)GETARG_C(ins);
        break;
      case OP_MUL:
        op->kind = LUAR_OP_MUL;
        op->a = (uint8_t)GETARG_A(ins);
        op->b = (uint8_t)GETARG_B(ins);
        op->c = (uint8_t)GETARG_C(ins);
        break;
      case OP_MMBIN:
      case OP_MMBINI:
      case OP_MMBINK:
        op->kind = LUAR_OP_MMBIN;
        break;
      default:
        return 0;
    }
    if (op->kind != LUAR_OP_MMBIN && is_control_reg(op->a, for_a))
      return 0;  /* never let the body rewrite the loop control slots */
  }
  fn = luar_emit_build(&d, &size);
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


int luar_jit_run (lua_State *L, Proto *p, void *base) {
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


int luar_has_jit (void) {
#if defined(LUAR_JIT_DISABLE)
  return 0;
#else
  return luar_emit_available();
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
