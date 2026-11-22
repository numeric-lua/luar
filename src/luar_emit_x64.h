/*
** This file has been pre-processed with DynASM.
** https://luajit.org/dynasm.html
** DynASM version 1.5.0, DynASM x64 version 1.5.0
** DO NOT EDIT! The original file is in "src/luar_emit_x64.dasc".
*/

#line 1 "src/luar_emit_x64.dasc"
/* DynASM x86-64 emitter for J2 numeric loops.
**
** Build (checked in as luar_emit_x64.h):
**   lua dynasm/dynasm.lua -D X64 src/luar_emit_x64.dasc \
**       > src/luar_emit_x64.h
**
** The generated function has the SysV shape int f(void *base), where base
** points at Lua stack register R0.  It runs the remaining iterations of a
** numeric for-loop directly on the 16-byte tagged TValue slots and returns
** 1 on success or 0 if a tag guard fails.
*/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define DASM_CHECKS 1
#include "dasm_proto.h"
#include "dasm_x86.h"
#include "luar_emit.h"

//|.arch x64
#if DASM_VERSION != 10500
#error "Version mismatch between DynASM and included encoding engine"
#endif
#line 26 "src/luar_emit_x64.dasc"
//|.section code
#define DASM_SECTION_CODE	0
#define DASM_MAXSECTION		1
#line 27 "src/luar_emit_x64.dasc"
//|.actionlist luar_emit_actions
static const unsigned char luar_emit_actions[3026] = {
  128,187,233,235,15,133,244,249,255,72,139,131,233,252,242,72,15,42,192,255,
  252,242,15,16,131,233,255,252,242,15,17,131,233,198,131,233,235,255,72,139,
  131,233,72,137,131,233,138,131,233,136,131,233,255,72,184,237,237,72,137,
  131,233,198,131,233,235,255,72,129,131,233,239,255,72,139,131,233,72,129,
  192,239,72,137,131,233,198,131,233,235,255,72,184,237,237,102,72,15,110,192,
  255,252,242,15,88,131,233,252,242,15,17,131,233,255,252,242,15,89,131,233,
  252,242,15,17,131,233,255,252,242,15,16,139,233,255,252,242,15,92,200,255,
  252,242,15,94,200,255,252,242,15,17,139,233,255,72,184,237,237,102,72,15,
  110,200,255,252,242,15,88,193,255,252,242,15,92,193,255,252,242,15,89,193,
  255,252,242,15,94,193,255,72,139,131,233,255,72,129,192,239,255,72,129,232,
  239,255,72,105,192,239,255,72,139,131,233,252,242,72,15,42,200,255,72,139,
  131,233,72,1,131,233,255,72,139,131,233,72,41,131,233,255,72,139,131,233,
  72,15,175,131,233,72,137,131,233,255,72,3,131,233,255,72,43,131,233,255,72,
  15,175,131,233,255,72,185,237,237,72,57,200,255,72,59,131,233,255,15,140,
  245,255,15,141,245,255,15,142,245,255,15,143,245,255,15,132,245,255,15,133,
  245,255,72,184,237,237,252,242,72,15,42,200,255,102,15,46,193,255,15,138,
  245,255,15,130,245,255,15,131,245,255,15,134,245,255,15,135,245,255,72,139,
  131,233,72,131,252,248,1,15,140,244,249,76,57,252,240,15,135,244,249,72,252,
  255,200,72,193,224,4,76,1,232,128,184,233,235,15,133,244,249,72,139,8,72,
  137,139,233,198,131,233,235,255,72,139,131,233,72,131,252,248,1,15,140,244,
  249,76,57,252,240,15,135,244,249,72,252,255,200,72,193,224,4,76,1,232,128,
  184,233,235,15,133,244,249,72,139,8,255,72,139,131,233,72,1,200,72,137,131,
  233,198,131,233,235,255,252,242,72,15,42,193,252,242,15,88,131,233,252,242,
  15,17,131,233,198,131,233,235,255,72,139,131,233,72,131,252,248,1,15,140,
  244,249,76,57,252,240,15,135,244,249,72,252,255,200,72,193,224,4,76,1,232,
  255,72,139,139,233,72,137,8,198,128,233,235,255,252,242,15,16,131,233,252,
  242,15,17,0,198,128,233,235,255,83,72,137,252,251,255,65,84,65,85,65,86,255,
  65,187,237,255,76,139,163,233,73,139,132,253,36,233,72,133,192,15,133,244,
  249,77,139,172,253,36,233,69,139,180,253,36,233,255,252,233,245,255,249,184,
  1,0,0,0,255,65,94,65,93,65,92,255,91,195,255,249,72,139,131,233,72,133,192,
  15,132,244,248,72,252,255,200,72,137,131,233,72,139,139,233,72,3,139,233,
  72,137,139,233,252,233,245,249,248,2,184,1,0,0,0,255,248,3,68,137,216,131,
  192,2,252,247,216,255,83,72,137,252,251,65,84,65,85,65,86,255,65,87,255,76,
  139,163,233,76,139,171,233,76,139,179,233,255,252,242,15,16,131,233,72,184,
  237,237,102,72,15,110,200,255,76,139,187,233,73,187,237,237,255,248,1,255,
  252,242,73,15,42,213,252,242,15,89,209,252,242,15,88,194,255,76,137,252,233,
  73,15,175,203,73,1,207,255,77,133,228,15,132,244,248,73,252,255,204,77,1,
  252,245,252,233,244,1,248,2,76,137,163,233,76,137,171,233,255,76,137,187,
  233,65,95,255,184,1,0,0,0,65,94,65,93,65,92,91,195,248,3,255,65,95,49,192,
  65,94,65,93,65,92,91,195,255,83,72,137,252,251,65,84,65,85,65,86,65,87,76,
  139,163,233,76,139,171,233,76,139,179,233,255,76,139,187,233,73,139,135,233,
  72,133,192,15,133,244,249,77,139,151,233,69,139,159,233,248,1,73,131,252,
  253,1,15,140,244,249,77,57,221,15,135,244,249,76,137,232,72,252,255,200,72,
  193,224,4,76,1,208,76,137,40,198,128,233,235,77,133,228,15,132,244,248,73,
  252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,163,233,255,76,137,171,
  233,184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,76,137,163,233,76,137,
  171,233,49,192,65,95,65,94,65,93,65,92,91,195,255,76,139,187,233,255,252,
  242,73,15,42,205,252,242,15,88,193,255,77,1,252,239,255,76,137,187,233,255,
  83,72,137,252,251,65,84,65,85,65,86,65,87,76,139,163,233,76,139,171,233,76,
  139,179,233,76,139,187,233,248,1,76,137,232,72,105,192,239,73,1,199,77,133,
  228,15,132,244,248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,
  163,233,76,137,171,233,76,137,187,233,184,1,0,0,0,65,95,65,94,65,93,65,92,
  91,195,248,3,49,192,65,95,65,94,65,93,65,92,91,195,255,83,72,137,252,251,
  65,84,65,85,65,86,65,87,76,139,163,233,76,139,171,233,76,139,179,233,76,139,
  187,233,248,1,77,1,252,239,77,133,228,15,132,244,248,73,252,255,204,77,1,
  252,245,252,233,244,1,248,2,76,137,163,233,76,137,171,233,76,137,187,233,
  184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,49,192,65,95,65,94,65,93,
  65,92,91,195,255,76,139,131,233,73,139,128,233,72,133,192,15,133,244,249,
  77,139,136,233,69,139,144,233,255,73,131,252,254,1,15,133,244,249,73,131,
  252,253,1,15,140,244,249,76,137,232,76,1,224,76,57,208,15,135,244,249,76,
  137,252,233,72,252,255,201,72,193,225,4,76,1,201,76,137,226,72,252,255,194,
  248,4,128,185,233,235,15,133,244,249,72,131,193,16,72,252,255,202,15,133,
  244,4,77,137,252,235,73,252,255,203,73,193,227,4,77,1,203,248,1,255,77,3,
  59,255,252,242,73,15,42,11,252,242,15,88,193,255,73,131,195,16,77,133,228,
  15,132,244,248,73,252,255,204,73,129,197,239,252,233,244,1,248,2,76,137,163,
  233,76,137,171,233,255,184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,76,
  137,163,233,76,137,171,233,255,252,242,15,16,131,233,72,184,237,237,102,72,
  15,110,208,255,76,139,163,233,255,76,139,171,233,255,73,190,237,237,255,248,
  1,77,57,252,245,255,15,140,244,248,255,15,141,244,248,255,15,142,244,248,
  255,15,143,244,248,255,252,242,73,15,42,205,255,252,242,15,89,201,252,242,
  15,88,193,255,252,242,15,89,202,252,242,15,88,193,255,77,15,175,229,255,77,
  41,252,236,255,76,137,252,233,73,15,175,205,73,1,204,255,77,1,252,236,255,
  73,129,197,239,252,233,244,1,248,2,255,76,137,171,233,184,1,0,0,0,65,94,65,
  93,65,92,91,195,248,3,49,192,65,94,65,93,65,92,91,195,255,252,242,73,15,42,
  205,252,242,15,89,201,252,242,15,88,193,255,76,137,252,233,73,15,175,205,
  73,1,207,255,77,15,175,252,253,255,252,242,73,15,42,205,252,242,15,92,193,
  255,77,41,252,239,255,83,72,137,252,251,65,85,65,86,65,87,76,139,171,233,
  255,248,1,73,129,197,239,255,77,57,252,238,255,15,140,244,1,255,15,141,244,
  1,255,15,142,244,1,255,15,143,244,1,255,15,132,244,1,255,15,133,244,1,255,
  184,1,0,0,0,65,95,65,94,65,93,91,195,248,3,248,2,49,192,65,95,65,94,65,93,
  91,195,255,184,237,91,195,248,3,49,192,91,195,255,72,139,131,233,72,133,192,
  15,132,244,248,72,252,255,200,72,137,131,233,72,139,139,233,72,3,139,233,
  72,137,139,233,252,233,244,1,248,2,184,1,0,0,0,91,195,248,3,49,192,91,195,
  255,65,84,65,87,83,72,131,252,236,16,72,137,252,251,73,137,252,244,65,191,
  0,1,0,0,65,187,237,248,1,255,65,187,237,128,187,233,235,15,133,244,248,255,
  72,141,187,233,190,237,72,184,237,237,252,255,208,65,187,237,133,192,15,132,
  244,248,255,72,141,187,233,72,190,237,237,76,137,226,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,76,137,231,72,141,179,233,72,186,
  237,237,185,237,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,
  248,255,72,141,187,233,72,141,179,233,72,141,147,233,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,141,179,233,72,
  184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,72,139,179,233,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,
  192,15,132,244,248,255,72,141,187,233,252,242,15,16,131,233,72,141,179,233,
  72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,76,137,231,
  72,141,179,233,186,237,185,237,72,184,237,237,252,255,208,65,187,237,133,
  192,15,132,244,248,255,72,141,187,233,72,141,179,233,76,137,226,72,184,237,
  237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,137,223,190,237,
  186,237,76,137,225,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,
  248,255,72,137,223,190,237,76,137,226,72,184,237,237,252,255,208,65,187,237,
  133,192,15,132,244,248,255,72,141,187,233,72,190,237,237,72,141,147,233,72,
  184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,72,141,179,233,72,141,147,233,76,137,225,72,184,237,237,252,255,208,65,
  187,237,72,133,192,15,132,244,248,72,137,195,255,191,237,72,190,237,237,72,
  184,237,237,102,72,15,110,192,76,137,226,72,184,237,237,252,255,208,65,187,
  237,133,192,15,132,244,248,255,72,141,187,233,72,190,237,237,72,141,147,233,
  76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,
  72,137,195,255,72,141,187,233,72,190,237,237,186,237,76,137,225,72,184,237,
  237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,195,255,72,141,
  187,233,72,190,237,237,72,186,237,237,76,137,225,72,184,237,237,252,255,208,
  65,187,237,72,133,192,15,132,244,248,72,137,195,255,72,141,187,233,72,199,
  198,237,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,
  244,248,255,72,141,187,233,72,199,198,237,72,141,147,233,76,137,225,72,184,
  237,237,252,255,208,65,187,237,72,133,192,15,132,244,248,72,137,195,255,72,
  141,187,233,139,179,233,139,147,233,72,141,139,233,72,184,237,237,252,255,
  208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,139,179,233,252,
  242,15,16,131,233,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,
  192,15,132,244,248,255,72,141,187,233,252,243,15,126,131,233,72,139,179,233,
  72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,
  255,72,141,187,233,252,242,15,16,131,233,72,139,179,233,72,141,147,233,72,
  184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,
  233,252,243,15,126,131,233,252,243,15,126,139,233,72,141,179,233,72,184,237,
  237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,252,
  242,15,16,131,233,252,242,15,16,139,233,72,141,179,233,72,184,237,237,252,
  255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,252,242,15,16,
  131,233,252,242,15,16,139,233,252,242,15,16,147,233,72,141,179,233,72,184,
  237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,
  72,139,179,233,72,141,147,233,72,184,237,237,252,255,208,65,187,237,133,192,
  15,132,244,248,198,131,233,235,255,72,141,187,233,72,199,198,237,72,141,147,
  233,72,141,139,233,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,
  248,255,72,141,187,233,72,199,198,237,72,199,194,237,255,72,199,193,237,255,
  72,141,187,233,72,199,198,237,72,199,194,237,72,141,139,233,72,184,237,237,
  252,255,208,65,187,237,133,192,15,132,244,248,255,72,141,187,233,72,139,179,
  233,72,141,147,233,255,72,184,237,237,255,72,141,187,233,72,139,179,233,72,
  141,147,233,76,137,225,72,184,237,237,252,255,208,65,187,237,72,133,192,15,
  132,244,248,72,137,195,255,65,187,237,72,139,131,233,72,185,237,237,72,57,
  200,15,133,244,254,255,72,141,187,233,72,141,179,233,72,184,237,237,102,72,
  15,110,192,72,184,237,237,252,255,208,65,187,237,133,192,15,132,244,248,255,
  65,187,237,252,233,244,254,255,72,139,131,233,72,133,192,15,132,244,251,72,
  252,255,200,72,137,131,233,72,139,139,233,72,3,139,233,72,137,139,233,49,
  192,72,131,196,16,91,65,95,65,92,195,248,5,184,1,0,0,0,72,131,196,16,91,65,
  95,65,92,195,255,72,139,131,233,72,133,192,15,132,244,249,72,252,255,200,
  72,137,131,233,72,139,139,233,72,3,139,233,72,137,139,233,65,252,255,207,
  15,132,244,252,252,233,244,1,255,248,3,184,1,0,0,0,72,131,196,16,91,65,95,
  65,92,195,248,2,255,68,137,216,252,247,216,131,232,2,255,72,131,196,16,91,
  65,95,65,92,195,248,6,184,2,0,0,0,72,131,196,16,91,65,95,65,92,195,248,8,
  68,137,216,252,247,216,131,232,2,72,131,196,16,91,65,95,65,92,195,255
};

#line 28 "src/luar_emit_x64.dasc"

#define LUAR_SLOT(r)	(16 * (int)(r))
#define LUAR_TAG(r)	(16 * (int)(r) + 8)

static void emit_guards (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  for (i = 0; i < d->nguards; i++) {
    int r = d->guard_reg[i];
    int tag = (d->guard_type[i] == LUAR_T_FLOAT) ? d->tag_flt :
              (d->guard_type[i] == LUAR_T_TABLE) ? d->tag_table :
              d->tag_int;
    //| cmp byte [rbx + ((int)LUAR_TAG(r))], ((int)tag)
    //| jne >3
    dasm_put(Dst, 0, ((int)LUAR_TAG(r)), ((int)tag));
#line 42 "src/luar_emit_x64.dasc"
  }
}

static void load_double (dasm_State **Dst, int reg, int type)
{
  if (type == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(reg))]
    //| cvtsi2sd xmm0, rax
    dasm_put(Dst, 9, ((int)LUAR_SLOT(reg)));
#line 50 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(reg))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(reg)));
#line 53 "src/luar_emit_x64.dasc"
  }
}

static void store_double (dasm_State **Dst, const LuaR_LoopDesc *d, int reg)
{
  //| movsd qword [rbx + ((int)LUAR_SLOT(reg))], xmm0
  //| mov byte [rbx + ((int)LUAR_TAG(reg))], ((int)d->tag_flt)
  dasm_put(Dst, 27, ((int)LUAR_SLOT(reg)), ((int)LUAR_TAG(reg)), ((int)d->tag_flt));
#line 60 "src/luar_emit_x64.dasc"
}

static void emit_body (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    int a = op->a, b = op->b;
    switch (op->kind) {
      case LUAR_OP_MOVE:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov al, byte [rbx + ((int)LUAR_TAG(b))]
        //| mov byte [rbx + ((int)LUAR_TAG(a))], al
        dasm_put(Dst, 38, ((int)LUAR_SLOT(b)), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(b)), ((int)LUAR_TAG(a)));
#line 74 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_LOADI:
        //| mov64 rax, ((int64_t)op->imm)
        //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
        dasm_put(Dst, 53, (unsigned int)(((int64_t)op->imm)), (unsigned int)((((int64_t)op->imm))>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 79 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_LOADF:
      case LUAR_OP_LOADK: {
        int64_t bits;
        if (op->kind == LUAR_OP_LOADF || op->ta == LUAR_T_FLOAT) {
          memcpy(&bits, &op->num, sizeof(bits));
          //| mov64 rax, bits
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_flt)
          dasm_put(Dst, 53, (unsigned int)(bits), (unsigned int)((bits)>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_flt));
#line 88 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov64 rax, op->imm
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 53, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 93 "src/luar_emit_x64.dasc"
        }
        break;
      }
      case LUAR_OP_ADDI:
        if (a == b) {
          //| add qword [rbx + ((int)LUAR_SLOT(a))], ((int64_t)op->imm)
          dasm_put(Dst, 66, ((int)LUAR_SLOT(a)), ((int64_t)op->imm));
#line 99 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          //| add rax, ((int64_t)op->imm)
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 72, ((int)LUAR_SLOT(b)), ((int64_t)op->imm), ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 105 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_ADDK:
      case LUAR_OP_SUBK:
      case LUAR_OP_MULK:
      case LUAR_OP_DIVK: {
        int64_t bits;
        int isdiv = op->kind == LUAR_OP_DIVK;
        memcpy(&bits, &op->num, sizeof(bits));
        if (a == b && op->tb == LUAR_T_FLOAT &&
            (isdiv || op->ta == LUAR_T_FLOAT)) {
          //| mov64 rax, bits
          //| movq xmm0, rax
          dasm_put(Dst, 89, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 118 "src/luar_emit_x64.dasc"
          if (op->kind == LUAR_OP_ADDK) {
            //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(a))]
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm0
            dasm_put(Dst, 99, ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 121 "src/luar_emit_x64.dasc"
          }
          else if (op->kind == LUAR_OP_MULK) {
            //| mulsd xmm0, qword [rbx + ((int)LUAR_SLOT(a))]
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm0
            dasm_put(Dst, 112, ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 125 "src/luar_emit_x64.dasc"
          }
          else {
            //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a))]
            dasm_put(Dst, 125, ((int)LUAR_SLOT(a)));
#line 128 "src/luar_emit_x64.dasc"
            if (op->kind == LUAR_OP_SUBK) {
              //| subsd xmm1, xmm0
              dasm_put(Dst, 132);
#line 130 "src/luar_emit_x64.dasc"
            }
            else {
              //| divsd xmm1, xmm0
              dasm_put(Dst, 138);
#line 133 "src/luar_emit_x64.dasc"
            }
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm1
            dasm_put(Dst, 144, ((int)LUAR_SLOT(a)));
#line 135 "src/luar_emit_x64.dasc"
          }
          break;
        }
        load_double(Dst, b, op->tb);
        if (isdiv || op->ta == LUAR_T_FLOAT) {
          //| mov64 rax, bits
          //| movq xmm1, rax
          dasm_put(Dst, 151, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 142 "src/luar_emit_x64.dasc"
          if (op->kind == LUAR_OP_ADDK) {
            //| addsd xmm0, xmm1
            dasm_put(Dst, 161);
#line 144 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_SUBK) {
            //| subsd xmm0, xmm1
            dasm_put(Dst, 167);
#line 147 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_MULK) {
            //| mulsd xmm0, xmm1
            dasm_put(Dst, 173);
#line 150 "src/luar_emit_x64.dasc"
          }
          if (isdiv) {
            //| divsd xmm0, xmm1
            dasm_put(Dst, 179);
#line 153 "src/luar_emit_x64.dasc"
          }
          store_double(Dst, d, a);
        }
        else {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          dasm_put(Dst, 185, ((int)LUAR_SLOT(b)));
#line 158 "src/luar_emit_x64.dasc"
          /* integer constant: op->imm holds the value */
          if (op->kind == LUAR_OP_ADDK) {
            //| add rax, ((int64_t)op->imm)
            dasm_put(Dst, 190, ((int64_t)op->imm));
#line 161 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_SUBK) {
            //| sub rax, ((int64_t)op->imm)
            dasm_put(Dst, 195, ((int64_t)op->imm));
#line 164 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_MULK) {
            //| imul rax, ((int64_t)op->imm)
            dasm_put(Dst, 200, ((int64_t)op->imm));
#line 167 "src/luar_emit_x64.dasc"
          }
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 170 "src/luar_emit_x64.dasc"
        }
        break;
      }
      case LUAR_OP_ADD:
      case LUAR_OP_SUB:
      case LUAR_OP_MUL:
      case LUAR_OP_DIV: {
        int isdiv = op->kind == LUAR_OP_DIV;
        int flt = isdiv || op->ta == LUAR_T_FLOAT || op->tb == LUAR_T_FLOAT;
        if (flt && a == b && op->tb == LUAR_T_FLOAT) {
          /* In-place float accumulator: tag stays float. */
          load_double(Dst, op->c, op->tc);
          if (op->kind == LUAR_OP_ADD) {
            //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(a))]
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm0
            dasm_put(Dst, 99, ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 185 "src/luar_emit_x64.dasc"
          }
          else if (op->kind == LUAR_OP_MUL) {
            //| mulsd xmm0, qword [rbx + ((int)LUAR_SLOT(a))]
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm0
            dasm_put(Dst, 112, ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 189 "src/luar_emit_x64.dasc"
          }
          else {
            //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a))]
            dasm_put(Dst, 125, ((int)LUAR_SLOT(a)));
#line 192 "src/luar_emit_x64.dasc"
            if (op->kind == LUAR_OP_SUB) {
              //| subsd xmm1, xmm0
              dasm_put(Dst, 132);
#line 194 "src/luar_emit_x64.dasc"
            }
            else {
              //| divsd xmm1, xmm0
              dasm_put(Dst, 138);
#line 197 "src/luar_emit_x64.dasc"
            }
            //| movsd qword [rbx + ((int)LUAR_SLOT(a))], xmm1
            dasm_put(Dst, 144, ((int)LUAR_SLOT(a)));
#line 199 "src/luar_emit_x64.dasc"
          }
        }
        else if (flt) {
          load_double(Dst, b, op->tb);
          if (op->tc == LUAR_T_INT) {
            //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            //| cvtsi2sd xmm1, rax
            dasm_put(Dst, 205, ((int)LUAR_SLOT(op->c)));
#line 206 "src/luar_emit_x64.dasc"
          }
          else {
            //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(op->c))]
            dasm_put(Dst, 125, ((int)LUAR_SLOT(op->c)));
#line 209 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_ADD) {
            //| addsd xmm0, xmm1
            dasm_put(Dst, 161);
#line 212 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_SUB) {
            //| subsd xmm0, xmm1
            dasm_put(Dst, 167);
#line 215 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_MUL) {
            //| mulsd xmm0, xmm1
            dasm_put(Dst, 173);
#line 218 "src/luar_emit_x64.dasc"
          }
          if (isdiv) {
            //| divsd xmm0, xmm1
            dasm_put(Dst, 179);
#line 221 "src/luar_emit_x64.dasc"
          }
          store_double(Dst, d, a);
        }
        else if (a == b) {
          /* In-place integer accumulator: the tag is already correct. */
          if (op->kind == LUAR_OP_ADD) {
            //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            //| add qword [rbx + ((int)LUAR_SLOT(a))], rax
            dasm_put(Dst, 216, ((int)LUAR_SLOT(op->c)), ((int)LUAR_SLOT(a)));
#line 229 "src/luar_emit_x64.dasc"
          }
          else if (op->kind == LUAR_OP_SUB) {
            //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            //| sub qword [rbx + ((int)LUAR_SLOT(a))], rax
            dasm_put(Dst, 225, ((int)LUAR_SLOT(op->c)), ((int)LUAR_SLOT(a)));
#line 233 "src/luar_emit_x64.dasc"
          }
          else {
            //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            //| imul rax, qword [rbx + ((int)LUAR_SLOT(a))]
            //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
            dasm_put(Dst, 234, ((int)LUAR_SLOT(op->c)), ((int)LUAR_SLOT(a)), ((int)LUAR_SLOT(a)));
#line 238 "src/luar_emit_x64.dasc"
          }
        }
        else {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
          dasm_put(Dst, 185, ((int)LUAR_SLOT(b)));
#line 242 "src/luar_emit_x64.dasc"
          if (op->kind == LUAR_OP_ADD) {
            //| add rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            dasm_put(Dst, 248, ((int)LUAR_SLOT(op->c)));
#line 244 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_SUB) {
            //| sub rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            dasm_put(Dst, 253, ((int)LUAR_SLOT(op->c)));
#line 247 "src/luar_emit_x64.dasc"
          }
          if (op->kind == LUAR_OP_MUL) {
            //| imul rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
            dasm_put(Dst, 258, ((int)LUAR_SLOT(op->c)));
#line 250 "src/luar_emit_x64.dasc"
          }
          //| mov qword [rbx + ((int)LUAR_SLOT(a))], rax
          //| mov byte [rbx + ((int)LUAR_TAG(a))], ((int)d->tag_int)
          dasm_put(Dst, 57, ((int)LUAR_SLOT(a)), ((int)LUAR_TAG(a)), ((int)d->tag_int));
#line 253 "src/luar_emit_x64.dasc"
        }
        break;
      }
      case LUAR_OP_MMBIN:
        break;  /* numeric fast path proved by the guards */
      default:
        break;
    }
  }
}

static void emit_int_one (dasm_State **Dst, const LuaR_LoopDesc *d,
                          const LuaR_Op *op)
{
  LuaR_LoopDesc one = *d;
  one.ops = op;
  one.nops = 1;
  emit_body(Dst, &one);
}

static void emit_cmp (dasm_State **Dst, const LuaR_LoopDesc *d,
                      const LuaR_Op *op, int fallthrough)
{
  int a = op->a, b = op->b;
  int k = op->k;
  int both_int = (op->ta == LUAR_T_INT && op->tb == LUAR_T_INT);
  (void)d;
  if (both_int) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(a))]
    dasm_put(Dst, 185, ((int)LUAR_SLOT(a)));
#line 282 "src/luar_emit_x64.dasc"
    if (b == 0xFF) {
      //| mov64 rcx, op->imm
      //| cmp rax, rcx
      dasm_put(Dst, 264, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 285 "src/luar_emit_x64.dasc"
    }
    else {
      //| cmp rax, qword [rbx + ((int)LUAR_SLOT(b))]
      dasm_put(Dst, 272, ((int)LUAR_SLOT(b)));
#line 288 "src/luar_emit_x64.dasc"
    }
    switch (op->kind) {
      case LUAR_OP_LT:
        if (k) {
          //| jl =>((int)op->target)
          dasm_put(Dst, 277, ((int)op->target));
#line 293 "src/luar_emit_x64.dasc"
        }
        else {
          //| jge =>((int)op->target)
          dasm_put(Dst, 281, ((int)op->target));
#line 296 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_LE:
        if (k) {
          //| jle =>((int)op->target)
          dasm_put(Dst, 285, ((int)op->target));
#line 301 "src/luar_emit_x64.dasc"
        }
        else {
          //| jg =>((int)op->target)
          dasm_put(Dst, 289, ((int)op->target));
#line 304 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_GT:
        if (k) {
          //| jg =>((int)op->target)
          dasm_put(Dst, 289, ((int)op->target));
#line 309 "src/luar_emit_x64.dasc"
        }
        else {
          //| jle =>((int)op->target)
          dasm_put(Dst, 285, ((int)op->target));
#line 312 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_GE:
        if (k) {
          //| jge =>((int)op->target)
          dasm_put(Dst, 281, ((int)op->target));
#line 317 "src/luar_emit_x64.dasc"
        }
        else {
          //| jl =>((int)op->target)
          dasm_put(Dst, 277, ((int)op->target));
#line 320 "src/luar_emit_x64.dasc"
        }
        break;
      case LUAR_OP_EQ:
        if (k) {
          //| je =>((int)op->target)
          dasm_put(Dst, 293, ((int)op->target));
#line 325 "src/luar_emit_x64.dasc"
        }
        else {
          //| jne =>((int)op->target)
          dasm_put(Dst, 297, ((int)op->target));
#line 328 "src/luar_emit_x64.dasc"
        }
        break;
      default: break;
    }
    return;
  }

  /* At least one operand is a double: convert both and use ucomisd. */
  load_double(Dst, a, op->ta);
  if (b == 0xFF) {
    if (op->tb == LUAR_T_INT) {
      //| mov64 rax, op->imm
      //| cvtsi2sd xmm1, rax
      dasm_put(Dst, 301, (unsigned int)(op->imm), (unsigned int)((op->imm)>>32));
#line 341 "src/luar_emit_x64.dasc"
    }
    else {
      int64_t bits;
      memcpy(&bits, &op->num, sizeof(bits));
      //| mov64 rax, bits
      //| movq xmm1, rax
      dasm_put(Dst, 151, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 347 "src/luar_emit_x64.dasc"
    }
  }
  else if (op->tb == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(b))]
    //| cvtsi2sd xmm1, rax
    dasm_put(Dst, 205, ((int)LUAR_SLOT(b)));
#line 352 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(b))]
    dasm_put(Dst, 125, ((int)LUAR_SLOT(b)));
#line 355 "src/luar_emit_x64.dasc"
  }
  //| ucomisd xmm0, xmm1
  dasm_put(Dst, 312);
#line 357 "src/luar_emit_x64.dasc"
  /* Unordered (NaN) comparisons are false, as in Lua. */
  if (k) {
    //| jp =>((int)fallthrough)
    dasm_put(Dst, 317, ((int)fallthrough));
#line 360 "src/luar_emit_x64.dasc"
  }
  else {
    //| jp =>((int)op->target)
    dasm_put(Dst, 317, ((int)op->target));
#line 363 "src/luar_emit_x64.dasc"
  }
  switch (op->kind) {
    case LUAR_OP_LT:
      if (k) {
          //| jb =>((int)op->target)
          dasm_put(Dst, 321, ((int)op->target));
#line 368 "src/luar_emit_x64.dasc"
        }
        else {
          //| jae =>((int)op->target)
          dasm_put(Dst, 325, ((int)op->target));
#line 371 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_LE:
      if (k) {
          //| jbe =>((int)op->target)
          dasm_put(Dst, 329, ((int)op->target));
#line 376 "src/luar_emit_x64.dasc"
        }
        else {
          //| ja =>((int)op->target)
          dasm_put(Dst, 333, ((int)op->target));
#line 379 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_GT:
      if (k) {
          //| ja =>((int)op->target)
          dasm_put(Dst, 333, ((int)op->target));
#line 384 "src/luar_emit_x64.dasc"
        }
        else {
          //| jbe =>((int)op->target)
          dasm_put(Dst, 329, ((int)op->target));
#line 387 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_GE:
      if (k) {
          //| jae =>((int)op->target)
          dasm_put(Dst, 325, ((int)op->target));
#line 392 "src/luar_emit_x64.dasc"
        }
        else {
          //| jb =>((int)op->target)
          dasm_put(Dst, 321, ((int)op->target));
#line 395 "src/luar_emit_x64.dasc"
        }
      break;
    case LUAR_OP_EQ:
      if (k) {
          //| je =>((int)op->target)
          dasm_put(Dst, 293, ((int)op->target));
#line 400 "src/luar_emit_x64.dasc"
        }
        else {
          //| jne =>((int)op->target)
          dasm_put(Dst, 297, ((int)op->target));
#line 403 "src/luar_emit_x64.dasc"
        }
      break;
    default: break;
  }
}

static void emit_table_get (dasm_State **Dst, const LuaR_LoopDesc *d,
                            const LuaR_Op *op)
{
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| dec rax
  //| shl rax, 4
  //| add rax, r13
  //| cmp byte [rax + 8], ((int)d->tag_int)
  //| jne >3
  //| mov rcx, qword [rax]
  //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rcx
  //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
  dasm_put(Dst, 337, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 425 "src/luar_emit_x64.dasc"
}

static void emit_table_add (dasm_State **Dst, const LuaR_LoopDesc *d,
                            const LuaR_Op *op)
{
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->c))]
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| dec rax
  //| shl rax, 4
  //| add rax, r13
  //| cmp byte [rax + 8], ((int)d->tag_int)
  //| jne >3
  //| mov rcx, qword [rax]
  dasm_put(Dst, 389, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int));
#line 441 "src/luar_emit_x64.dasc"
  if (op->ta == LUAR_T_INT) {
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| add rax, rcx
    //| mov qword [rbx + ((int)LUAR_SLOT(op->a))], rax
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_int)
    dasm_put(Dst, 433, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_int));
#line 446 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm0, rcx
    //| addsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->a))]
    //| movsd qword [rbx + ((int)LUAR_SLOT(op->a))], xmm0
    //| mov byte [rbx + ((int)LUAR_TAG(op->a))], ((int)d->tag_flt)
    dasm_put(Dst, 449, ((int)LUAR_SLOT(op->a)), ((int)LUAR_SLOT(op->a)), ((int)LUAR_TAG(op->a)), ((int)d->tag_flt));
#line 452 "src/luar_emit_x64.dasc"
  }
}

static void emit_table_set (dasm_State **Dst, const LuaR_LoopDesc *d,
                            const LuaR_Op *op)
{
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(op->b))]
  //| cmp rax, 1
  //| jl >3
  //| cmp rax, r14
  //| ja >3
  //| dec rax
  //| shl rax, 4
  //| add rax, r13
  dasm_put(Dst, 472, ((int)LUAR_SLOT(op->b)));
#line 466 "src/luar_emit_x64.dasc"
  if (op->ta == LUAR_T_INT) {
    //| mov rcx, qword [rbx + ((int)LUAR_SLOT(op->c))]
    //| mov qword [rax], rcx
    //| mov byte [rax + 8], ((int)d->tag_int)
    dasm_put(Dst, 505, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_int));
#line 470 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(op->c))]
    //| movsd qword [rax], xmm0
    //| mov byte [rax + 8], ((int)d->tag_flt)
    dasm_put(Dst, 517, ((int)LUAR_SLOT(op->c)), 8, ((int)d->tag_flt));
#line 475 "src/luar_emit_x64.dasc"
  }
}

static void emit_branch_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int i;
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 533);
#line 483 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| push r12
    //| push r13
    //| push r14
    dasm_put(Dst, 539);
#line 487 "src/luar_emit_x64.dasc"
  }
  //| mov r11d, ((int)d->entry_pc)
  dasm_put(Dst, 546, ((int)d->entry_pc));
#line 489 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  if (d->has_table) {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->table_reg))]
    //| mov rax, qword [r12 + ((int)d->table_metatable_off)]
    //| test rax, rax
    //| jnz >3
    //| mov r13, qword [r12 + ((int)d->table_array_off)]
    //| mov r14d, dword [r12 + ((int)d->table_asize_off)]
    dasm_put(Dst, 550, ((int)LUAR_SLOT(d->table_reg)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 497 "src/luar_emit_x64.dasc"
  }
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    //|=>((int)i):
    dasm_put(Dst, 7, ((int)i));
#line 501 "src/luar_emit_x64.dasc"
    if (op->kind == LUAR_OP_GETTABLE || op->kind == LUAR_OP_TABLE_ADD ||
        op->kind == LUAR_OP_SETTABLE)
      //| mov r11d, ((int)op->pc)
      dasm_put(Dst, 546, ((int)op->pc));
#line 504 "src/luar_emit_x64.dasc"
    switch (op->kind) {
      case LUAR_OP_LT:
      case LUAR_OP_LE:
      case LUAR_OP_GT:
      case LUAR_OP_GE:
      case LUAR_OP_EQ:
        emit_cmp(Dst, d, op, i + 2);
        /* false/true fall-through skips the paired JMP; repeat loops
        ** exit at the descriptor's end label instead. */
        if (d->repeat && i == d->nops - 1)
          //| jmp =>((int)(d->nops + 1))
          dasm_put(Dst, 580, ((int)(d->nops + 1)));
#line 515 "src/luar_emit_x64.dasc"
        else
          //| jmp =>((int)(i + 2))
          dasm_put(Dst, 580, ((int)(i + 2)));
#line 517 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_JMP:
        //| jmp =>((int)op->target)
        dasm_put(Dst, 580, ((int)op->target));
#line 520 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_MMBIN:
      case LUAR_OP_NOP:
        break;
      case LUAR_OP_GETTABLE:
        emit_table_get(Dst, d, op);
        break;
      case LUAR_OP_TABLE_ADD:
        emit_table_add(Dst, d, op);
        break;
      case LUAR_OP_SETTABLE:
        emit_table_set(Dst, d, op);
        break;
      default:
        emit_int_one(Dst, d, op);
        break;
    }
  }
  if (d->is_while) {
    if (!d->repeat)
      //| jmp =>0
      dasm_put(Dst, 580, 0);
#line 541 "src/luar_emit_x64.dasc"
    if (d->repeat)
      //|=>((int)(d->nops + 1)):
      dasm_put(Dst, 7, ((int)(d->nops + 1)));
#line 543 "src/luar_emit_x64.dasc"
    else
      //|=>((int)d->nops):
    //| mov eax, 1
    dasm_put(Dst, 584, ((int)d->nops));
#line 546 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 591);
#line 550 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 598);
#line 553 "src/luar_emit_x64.dasc"
  }
  else {
    //|=>((int)d->nops):
    //| mov rax, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
    //| test rax, rax
    //| jz >2
    //| dec rax
    //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], rax
    //| mov rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
    //| add rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
    //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], rcx
    //| jmp =>0
    //|=>((int)(d->nops + 1)):
    //|2:
    //| mov eax, 1
    dasm_put(Dst, 601, ((int)d->nops), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)), 0, ((int)(d->nops + 1)));
#line 568 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 591);
#line 572 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 598);
#line 575 "src/luar_emit_x64.dasc"
  }
  //|3:
  //| mov eax, r11d
  //| add eax, 2
  //| neg eax
  dasm_put(Dst, 645);
#line 580 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| pop r14
    //| pop r13
    //| pop r12
    dasm_put(Dst, 591);
#line 584 "src/luar_emit_x64.dasc"
  }
  //| pop rbx
  //| ret
  dasm_put(Dst, 598);
#line 587 "src/luar_emit_x64.dasc"
}

static void emit_fast_float (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int64_t bits;
  memcpy(&bits, &d->ff_const, sizeof(bits));
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  dasm_put(Dst, 657);
#line 598 "src/luar_emit_x64.dasc"
  if (!d->ff_is_float)
    //| push r15
    dasm_put(Dst, 669);
#line 600 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->ff_idx))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 672, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ff_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 604 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
    //| mov64 rax, bits
    //| movq xmm1, rax
    dasm_put(Dst, 685, ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 608 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
    //| mov64 r11, d->ff_iconst
    dasm_put(Dst, 701, ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(d->ff_iconst), (unsigned int)((d->ff_iconst)>>32));
#line 612 "src/luar_emit_x64.dasc"
  }
  //|1:
  dasm_put(Dst, 710);
#line 614 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| cvtsi2sd xmm2, r13
    //| mulsd xmm2, xmm1
    //| addsd xmm0, xmm2
    dasm_put(Dst, 713);
#line 618 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov rcx, r13
    //| imul rcx, r11
    //| add r15, rcx
    dasm_put(Dst, 730);
#line 623 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 742, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 632 "src/luar_emit_x64.dasc"
  if (d->ff_is_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ff_acc)));
#line 634 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], r15
    //| pop r15
    dasm_put(Dst, 772, ((int)LUAR_SLOT(d->ff_acc)));
#line 638 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  dasm_put(Dst, 779);
#line 646 "src/luar_emit_x64.dasc"
  if (!d->ff_is_float)
    //| pop r15
  //| xor eax, eax
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 795);
#line 654 "src/luar_emit_x64.dasc"
}

static void emit_fast_fill (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 808, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 667 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ff_table))]
  //| mov rax, qword [r15 + ((int)d->table_metatable_off)]
  //| test rax, rax
  //| jnz >3
  //| mov r10, qword [r15 + ((int)d->table_array_off)]
  //| mov r11d, dword [r15 + ((int)d->table_asize_off)]
  //|1:
  //| cmp r13, 1
  //| jl >3
  //| cmp r13, r11
  //| ja >3
  //| mov rax, r13
  //| dec rax
  //| shl rax, 4
  //| add rax, r10
  //| mov qword [rax], r13
  //| mov byte [rax + 8], ((int)d->tag_int)
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 834, ((int)LUAR_SLOT(d->ff_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off), 8, ((int)d->tag_int), ((int)LUAR_SLOT(d->for_a)));
#line 693 "src/luar_emit_x64.dasc"
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 922, ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 710 "src/luar_emit_x64.dasc"
}

static void emit_fast_int (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 808, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 723 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fi_acc)));
#line 725 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
    dasm_put(Dst, 964, ((int)LUAR_SLOT(d->fi_acc)));
#line 728 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 710);
#line 731 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| cvtsi2sd xmm1, r13
    //| addsd xmm0, xmm1
    dasm_put(Dst, 969);
#line 734 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, r13
    dasm_put(Dst, 981);
#line 737 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 742, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 746 "src/luar_emit_x64.dasc"
  if (d->fi_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fi_acc)));
#line 748 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], r15
    dasm_put(Dst, 986, ((int)LUAR_SLOT(d->fi_acc)));
#line 751 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 926, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 769 "src/luar_emit_x64.dasc"
}

static void emit_fast_meta (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fm_acc))]
  //|1:
  //| mov rax, r13
  //| imul rax, ((int64_t)d->fm_mult)
  //| add r15, rax
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fm_acc))], r15
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 991, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fm_acc)), ((int64_t)d->fm_mult), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fm_acc)));
#line 811 "src/luar_emit_x64.dasc"
}


static void emit_fast_closure (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fc_upval_slot))]
  //|1:
  //| add r15, r13
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fc_upval_slot))], r15
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1095, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fc_upval_slot)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fc_upval_slot)));
#line 852 "src/luar_emit_x64.dasc"
}


static void emit_fast_tsum (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int isf = (d->ops[0].ta == LUAR_T_FLOAT);
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->ft_idx))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 808, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ft_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 867 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 964, ((int)LUAR_SLOT(d->ft_acc)));
#line 869 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->ft_acc)));
#line 872 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //| mov r8, qword [rbx + ((int)LUAR_SLOT(d->ft_table))]
  //| mov rax, qword [r8 + ((int)d->table_metatable_off)]
  //| test rax, rax
  //| jnz >3
  //| mov r9, qword [r8 + ((int)d->table_array_off)]
  //| mov r10d, dword [r8 + ((int)d->table_asize_off)]
  dasm_put(Dst, 1193, ((int)LUAR_SLOT(d->ft_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 880 "src/luar_emit_x64.dasc"
  /* This fast path only handles step 1 and pre-validates the whole range. */
  //| cmp r14, 1
  //| jne >3
  //| cmp r13, 1
  //| jl >3
  //| mov rax, r13
  //| add rax, r12
  //| cmp rax, r10
  //| ja >3
  //| mov rcx, r13
  //| dec rcx
  //| shl rcx, 4
  //| add rcx, r9
  //| mov rdx, r12
  //| inc rdx
  //|4:
  //| cmp byte [rcx + 8], ((int)d->tag_int)
  //| jne >3
  //| add rcx, 16
  //| dec rdx
  //| jnz <4
  //| mov r11, r13
  //| dec r11
  //| shl r11, 4
  //| add r11, r9
  //|1:
  dasm_put(Dst, 1217, 8, ((int)d->tag_int));
#line 906 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| add r15, qword [r11]
    dasm_put(Dst, 1310);
#line 908 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm1, qword [r11]
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1314);
#line 912 "src/luar_emit_x64.dasc"
  }
  //| add r11, 16
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, d->fw_step
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1326, d->fw_step, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 922 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 986, ((int)LUAR_SLOT(d->ft_acc)));
#line 924 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 927 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 1360, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 938 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 986, ((int)LUAR_SLOT(d->ft_acc)));
#line 940 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 943 "src/luar_emit_x64.dasc"
  }
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 951);
#line 951 "src/luar_emit_x64.dasc"
}

static void emit_fast_while (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int is_lt = (d->fw_kind == LUAR_OP_LT);
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  dasm_put(Dst, 657);
#line 961 "src/luar_emit_x64.dasc"
  if (d->fw_float) {
    union { double d; uint64_t u; } cv;
    cv.d = d->fw_const;
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
    //| mov64 rax, cv.u
    //| movq xmm2, rax
    dasm_put(Dst, 1386, ((int)LUAR_SLOT(d->fw_acc)), (unsigned int)(cv.u), (unsigned int)((cv.u)>>32));
#line 967 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
    dasm_put(Dst, 1402, ((int)LUAR_SLOT(d->fw_acc)));
#line 970 "src/luar_emit_x64.dasc"
  }
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->fw_idx))]
  dasm_put(Dst, 1407, ((int)LUAR_SLOT(d->fw_idx)));
#line 972 "src/luar_emit_x64.dasc"
  if (d->fw_limit == 0xFF) {
    //| mov64 r14, d->fw_imm
    dasm_put(Dst, 1412, (unsigned int)(d->fw_imm), (unsigned int)((d->fw_imm)>>32));
#line 974 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fw_limit))]
    dasm_put(Dst, 680, ((int)LUAR_SLOT(d->fw_limit)));
#line 977 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| cmp r13, r14
  dasm_put(Dst, 1417);
#line 981 "src/luar_emit_x64.dasc"
  if (is_lt) {
    if (d->fw_k) {
      //| jl >2
      dasm_put(Dst, 1424);
#line 984 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge >2
      dasm_put(Dst, 1429);
#line 987 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (d->fw_k) {
      //| jle >2
      dasm_put(Dst, 1434);
#line 992 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg >2
      dasm_put(Dst, 1439);
#line 995 "src/luar_emit_x64.dasc"
    }
  }
  if (d->fw_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1444);
#line 999 "src/luar_emit_x64.dasc"
    if (d->fw_prod) {
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1001 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_sub) {
      //| subsd xmm0, xmm1
      dasm_put(Dst, 167);
#line 1004 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_div) {
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1007 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_square) {
      //| mulsd xmm1, xmm1
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1451);
#line 1011 "src/luar_emit_x64.dasc"
    }
    else if (d->fw_linear) {
      //| mulsd xmm1, xmm2
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1462);
#line 1015 "src/luar_emit_x64.dasc"
    }
    else {
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1018 "src/luar_emit_x64.dasc"
    }
  }
  else if (d->fw_prod) {
    //| imul r12, r13
    dasm_put(Dst, 1473);
#line 1022 "src/luar_emit_x64.dasc"
  }
  else if (d->fw_sub) {
    //| sub r12, r13
    dasm_put(Dst, 1478);
#line 1025 "src/luar_emit_x64.dasc"
  }
  else if (d->fw_square) {
    //| mov rcx, r13
    //| imul rcx, r13
    //| add r12, rcx
    dasm_put(Dst, 1483);
#line 1030 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r12, r13
    dasm_put(Dst, 1495);
#line 1033 "src/luar_emit_x64.dasc"
  }
  //| add r13, d->fw_step
  //| jmp <1
  //|2:
  dasm_put(Dst, 1500, d->fw_step);
#line 1037 "src/luar_emit_x64.dasc"
  if (d->fw_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fw_acc)));
#line 1039 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], r12
    dasm_put(Dst, 917, ((int)LUAR_SLOT(d->fw_acc)));
#line 1042 "src/luar_emit_x64.dasc"
  }
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_idx))], r13
  //| mov eax, 1
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1511, ((int)LUAR_SLOT(d->fw_idx)));
#line 1057 "src/luar_emit_x64.dasc"
}

static void emit_fast_sq (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 808, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1070 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fs_acc)));
#line 1072 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
    dasm_put(Dst, 964, ((int)LUAR_SLOT(d->fs_acc)));
#line 1075 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 710);
#line 1078 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| cvtsi2sd xmm1, r13
    //| mulsd xmm1, xmm1
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1541);
#line 1082 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov rcx, r13
    //| imul rcx, r13
    //| add r15, rcx
    dasm_put(Dst, 1558);
#line 1087 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 742, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1096 "src/luar_emit_x64.dasc"
  if (d->fs_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fs_acc)));
#line 1098 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], r15
    dasm_put(Dst, 986, ((int)LUAR_SLOT(d->fs_acc)));
#line 1101 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1065);
#line 1117 "src/luar_emit_x64.dasc"
}

static void emit_fast_prod (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 808, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1130 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fp_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fp_acc)));
#line 1132 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fp_acc))]
    dasm_put(Dst, 964, ((int)LUAR_SLOT(d->fp_acc)));
#line 1135 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 710);
#line 1138 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1444);
#line 1140 "src/luar_emit_x64.dasc"
    if (d->fp_div)
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1142 "src/luar_emit_x64.dasc"
    else
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1144 "src/luar_emit_x64.dasc"
  }
  else {
    //| imul r15, r13
    dasm_put(Dst, 1570);
#line 1147 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 742, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1156 "src/luar_emit_x64.dasc"
  if (d->fp_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fp_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fp_acc)));
#line 1158 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fp_acc))], r15
    dasm_put(Dst, 986, ((int)LUAR_SLOT(d->fp_acc)));
#line 1161 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1065);
#line 1177 "src/luar_emit_x64.dasc"
}

static void emit_fast_sub (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| push r15
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  dasm_put(Dst, 808, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 1190 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1192 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))]
    dasm_put(Dst, 964, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1195 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 710);
#line 1198 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| cvtsi2sd xmm1, r13
    //| subsd xmm0, xmm1
    dasm_put(Dst, 1576);
#line 1201 "src/luar_emit_x64.dasc"
  }
  else {
    //| sub r15, r13
    dasm_put(Dst, 1588);
#line 1204 "src/luar_emit_x64.dasc"
  }
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  dasm_put(Dst, 742, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1213 "src/luar_emit_x64.dasc"
  if (d->fsb_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1215 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fsb_acc))], r15
    dasm_put(Dst, 986, ((int)LUAR_SLOT(d->fsb_acc)));
#line 1218 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 1065);
#line 1234 "src/luar_emit_x64.dasc"
}

static void emit_fast_repeat (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int kind = d->fr_kind, k = d->fr_k;
  //| push rbx
  //| mov rbx, rdi
  //| push r13
  //| push r14
  //| push r15
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->fr_idx))]
  dasm_put(Dst, 1593, ((int)LUAR_SLOT(d->fr_idx)));
#line 1245 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->fr_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->fr_acc)));
#line 1247 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fr_acc))]
    dasm_put(Dst, 964, ((int)LUAR_SLOT(d->fr_acc)));
#line 1250 "src/luar_emit_x64.dasc"
  }
  if (d->fr_limit != 0xFF) {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fr_limit))]
    dasm_put(Dst, 680, ((int)LUAR_SLOT(d->fr_limit)));
#line 1253 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov64 r14, d->fr_immediate
    dasm_put(Dst, 1412, (unsigned int)(d->fr_immediate), (unsigned int)((d->fr_immediate)>>32));
#line 1256 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| add r13, d->fr_step
  dasm_put(Dst, 1609, d->fr_step);
#line 1260 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| cvtsi2sd xmm1, r13
    dasm_put(Dst, 1444);
#line 1262 "src/luar_emit_x64.dasc"
    if (d->fr_op == 1) {
      //| subsd xmm0, xmm1
      dasm_put(Dst, 167);
#line 1264 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 2) {
      //| mulsd xmm0, xmm1
      dasm_put(Dst, 173);
#line 1267 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 3) {
      //| mulsd xmm1, xmm1
      //| addsd xmm0, xmm1
      dasm_put(Dst, 1451);
#line 1271 "src/luar_emit_x64.dasc"
    }
    else if (d->fr_op == 4) {
      //| divsd xmm0, xmm1
      dasm_put(Dst, 179);
#line 1274 "src/luar_emit_x64.dasc"
    }
    else {
      //| addsd xmm0, xmm1
      dasm_put(Dst, 161);
#line 1277 "src/luar_emit_x64.dasc"
    }
  }
  else if (d->fr_op == 1) {
    //| sub r15, r13
    dasm_put(Dst, 1588);
#line 1281 "src/luar_emit_x64.dasc"
  }
  else if (d->fr_op == 2) {
    //| imul r15, r13
    dasm_put(Dst, 1570);
#line 1284 "src/luar_emit_x64.dasc"
  }
  else if (d->fr_op == 3) {
    //| mov rcx, r13
    //| imul rcx, r13
    //| add r15, rcx
    dasm_put(Dst, 1558);
#line 1289 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r15, r13
    dasm_put(Dst, 981);
#line 1292 "src/luar_emit_x64.dasc"
  }
  if (d->fr_idx_left) {
    //| cmp r13, r14
    dasm_put(Dst, 1419);
#line 1295 "src/luar_emit_x64.dasc"
  }
  else {
    //| cmp r14, r13
    dasm_put(Dst, 1616);
#line 1298 "src/luar_emit_x64.dasc"
  }
  if (kind == LUAR_OP_LT) {
    if (k) {
      //| jl <1
      dasm_put(Dst, 1621);
#line 1302 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge <1
      dasm_put(Dst, 1626);
#line 1305 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_LE) {
    if (k) {
      //| jle <1
      dasm_put(Dst, 1631);
#line 1310 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg <1
      dasm_put(Dst, 1636);
#line 1313 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_GT) {
    if (k) {
      //| jg <1
      dasm_put(Dst, 1636);
#line 1318 "src/luar_emit_x64.dasc"
    }
    else {
      //| jle <1
      dasm_put(Dst, 1631);
#line 1321 "src/luar_emit_x64.dasc"
    }
  }
  else if (kind == LUAR_OP_GE) {
    if (k) {
      //| jge <1
      dasm_put(Dst, 1626);
#line 1326 "src/luar_emit_x64.dasc"
    }
    else {
      //| jl <1
      dasm_put(Dst, 1621);
#line 1329 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (k) {
      //| je <1
      dasm_put(Dst, 1641);
#line 1334 "src/luar_emit_x64.dasc"
    }
    else {
      //| jne <1
      dasm_put(Dst, 1646);
#line 1337 "src/luar_emit_x64.dasc"
    }
  }
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_idx))], r13
  dasm_put(Dst, 767, ((int)LUAR_SLOT(d->fr_idx)));
#line 1340 "src/luar_emit_x64.dasc"
  if (d->fr_float) {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->fr_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->fr_acc)));
#line 1342 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->fr_acc))], r15
    dasm_put(Dst, 986, ((int)LUAR_SLOT(d->fr_acc)));
#line 1345 "src/luar_emit_x64.dasc"
  }
  //| mov eax, 1
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop rbx
  //| ret
  //|3:
  //|2:
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop rbx
  //| ret
  dasm_put(Dst, 1651);
#line 1360 "src/luar_emit_x64.dasc"
}

static void emit_partial_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 533);
#line 1366 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  emit_body(Dst, d);
  //| mov eax, -((int)d->exit_pc + 2)
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop rbx
  //| ret
  dasm_put(Dst, 1679, -((int)d->exit_pc + 2));
#line 1375 "src/luar_emit_x64.dasc"
}

static void emit_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  if (d->partial) {
    emit_partial_code(Dst, d);
    return;
  }
  if (d->fast_repeat) {
    emit_fast_repeat(Dst, d);
    return;
  }
  if (d->fast_sub) {
    emit_fast_sub(Dst, d);
    return;
  }
  if (d->fast_prod) {
    emit_fast_prod(Dst, d);
    return;
  }
  if (d->fast_sq) {
    emit_fast_sq(Dst, d);
    return;
  }
  if (d->fast_while) {
    emit_fast_while(Dst, d);
    return;
  }
  if (d->fast_tsum) {
    emit_fast_tsum(Dst, d);
    return;
  }
  if (d->fast_meta) {
    emit_fast_meta(Dst, d);
    return;
  }
  if (d->fast_closure) {
    emit_fast_closure(Dst, d);
    return;
  }
  if (d->fast_int) {
    emit_fast_int(Dst, d);
    return;
  }
  if (d->fast_fill) {
    emit_fast_fill(Dst, d);
    return;
  }
  if (d->fast_float) {
    emit_fast_float(Dst, d);
    return;
  }
  if (d->has_branch) {
    emit_branch_code(Dst, d);
    return;
  }
  //| push rbx
  //| mov rbx, rdi
  dasm_put(Dst, 533);
#line 1433 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 710);
#line 1435 "src/luar_emit_x64.dasc"
  emit_body(Dst, d);
  /* Exact FORLOOP semantics: while count > 0, decrement, advance the index
  ** and run the body again.  When count is already zero the body just ran
  ** for the final iteration and the index must not advance again. */
  //| mov rax, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| test rax, rax
  //| jz >2
  //| dec rax
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], rax
  //| mov rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))]
  //| add rcx, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], rcx
  //| jmp <1
  //|2:
  //| mov eax, 1
  //| pop rbx
  //| ret
  //|3:
  //| xor eax, eax
  //| pop rbx
  //| ret
  dasm_put(Dst, 1690, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 1456 "src/luar_emit_x64.dasc"
}

static int emit_trace_ir_func (dasm_State **Dst, const LuaR_TraceIR *t) {
  int i, forslot = -1;
  int side = t->side;
  //| push r12
  //| push r15
  //| push rbx
  //| sub rsp, 16
  //| mov rbx, rdi
  //| mov r12, rsi
  //| mov r15d, 256
  //| mov r11d, ((int)t->startpc)
  //|1:
  dasm_put(Dst, 1741, ((int)t->startpc));
#line 1470 "src/luar_emit_x64.dasc"
  for (i = 0; i < t->nins; i++) {
    const LuaR_IRIns *in = &t->ins[i];
    switch (in->kind) {
      case LUAR_IR_GUARD: {
        int tag = (in->b == LUAR_IR_TAG_INT) ? LUA_VNUMINT :
                  (in->b == LUAR_IR_TAG_FLOAT) ? LUA_VNUMFLT :
                  (in->b == LUAR_IR_TAG_TABLE) ? ctb(LUA_VTABLE) :
                  (in->b == LUAR_IR_TAG_STRING) ? ctb(LUA_VSHRSTR) :
                  (in->b == LUAR_IR_TAG_USERDATA) ? ctb(LUA_VUSERDATA) : 0;
        if (tag == 0)
          return 0;
        //| mov r11d, ((int)in->pc)
        //| cmp byte [rbx + ((int)LUAR_TAG(in->a))], tag
        //| jne >2
        dasm_put(Dst, 1771, ((int)in->pc), ((int)LUAR_TAG(in->a)), tag);
#line 1484 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_LOADI:
        //| mov64 rax, in->ival
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 53, (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1490 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_KBOOL:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov esi, ((int)in->nval)
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_kbool
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1783, ((int)LUAR_SLOT(in->a)), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_kbool), (unsigned int)(((int64_t)(intptr_t)&luar_trace_kbool)>>32), ((int)in->pc));
#line 1499 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_KSTR:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_kstr
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1806, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_kstr), (unsigned int)(((int64_t)(intptr_t)&luar_trace_kstr)>>32), ((int)in->pc));
#line 1509 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_LOADF: {
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| mov64 rax, bits
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMFLT
        dasm_put(Dst, 53, (unsigned int)(bits), (unsigned int)((bits)>>32), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMFLT);
#line 1516 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_ADDI:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| add rax, in->ival
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 72, ((int)LUAR_SLOT(in->b)), in->ival, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1523 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_SLOAD:
      case LUAR_IR_SSTORE:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov al, byte [rbx + ((int)LUAR_TAG(in->b))]
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], al
        dasm_put(Dst, 38, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->b)), ((int)LUAR_TAG(in->a)));
#line 1530 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ULOAD:
        //| mov rdi, r12
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rdx, in->ival
        //| mov ecx, ((int)in->nval)
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_uload
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1834, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_uload), (unsigned int)(((int64_t)(intptr_t)&luar_trace_uload)>>32), ((int)in->pc));
#line 1541 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_ADD:
      case LUAR_IR_SUB:
      case LUAR_IR_MUL:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(in->b)));
#line 1546 "src/luar_emit_x64.dasc"
        if (in->kind == LUAR_IR_ADD) {
          //| add rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 248, ((int)LUAR_SLOT(in->c)));
#line 1548 "src/luar_emit_x64.dasc"
        }
        else if (in->kind == LUAR_IR_SUB) {
          //| sub rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 253, ((int)LUAR_SLOT(in->c)));
#line 1551 "src/luar_emit_x64.dasc"
        }
        else {
          //| imul rax, qword [rbx + ((int)LUAR_SLOT(in->c))]
          dasm_put(Dst, 258, ((int)LUAR_SLOT(in->c)));
#line 1554 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 57, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 1557 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CGETF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_cget_double
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1864, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cget_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cget_double)>>32), ((int)in->pc));
#line 1567 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CPSETI:
      case LUAR_IR_CPSETF:
      case LUAR_IR_CCALL0D:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall0_double
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1893, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall0_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall0_double)>>32), ((int)in->pc));
#line 1578 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL0I:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall0_int
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1893, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall0_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall0_int)>>32), ((int)in->pc));
#line 1587 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL1DI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall1di
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1918, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1di), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1di)>>32), ((int)in->pc));
#line 1597 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL1DD:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall1dd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1947, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1dd)>>32), ((int)in->pc));
#line 1607 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL1FF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall1ff
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1947, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1ff), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1ff)>>32), ((int)in->pc));
#line 1617 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TNEW:
        //| mov rdi, r12
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov edx, ((int)in->ival)
        //| mov ecx, ((int)in->nval)
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tnew
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1978, ((int)LUAR_SLOT(in->a)), ((int)in->ival), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tnew), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tnew)>>32), ((int)in->pc));
#line 1628 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TLEN:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tlen
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2006, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tlen), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tlen)>>32), ((int)in->pc));
#line 1638 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_STRLEN:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_strlen
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1893, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_strlen), (unsigned int)(((int64_t)(intptr_t)&luar_trace_strlen)>>32), ((int)in->pc));
#line 1647 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CONCATN:
        //| mov rdi, rbx
        //| mov esi, ((int)in->a)
        //| mov edx, ((int)in->ival)
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_concatn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2034, ((int)in->a), ((int)in->ival), (unsigned int)((int64_t)(intptr_t)&luar_trace_concatn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concatn)>>32), ((int)in->pc));
#line 1658 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CONCAT3:
        //| mov rdi, rbx
        //| mov esi, ((int)in->a)
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_concat3
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2061, ((int)in->a), (unsigned int)((int64_t)(intptr_t)&luar_trace_concat3), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concat3)>>32), ((int)in->pc));
#line 1668 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CONCAT2:
        //| mov rdi, rbx
        //| mov esi, ((int)in->a)
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_concat2
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2061, ((int)in->a), (unsigned int)((int64_t)(intptr_t)&luar_trace_concat2), (unsigned int)(((int64_t)(intptr_t)&luar_trace_concat2)>>32), ((int)in->pc));
#line 1678 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETKT:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetkt
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2086, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkt)>>32), ((int)in->pc));
#line 1688 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETKB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetkb
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2086, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkb)>>32), ((int)in->pc));
#line 1698 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETKS:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetks
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2086, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetks)>>32), ((int)in->pc));
#line 1708 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETKF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2086, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetkf)>>32), ((int)in->pc));
#line 1718 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETK:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2086, ((int)LUAR_SLOT(in->b)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetk)>>32), ((int)in->pc));
#line 1728 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETDYN:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetdyn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2115, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetdyn), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetdyn)>>32), ((int)in->pc));
#line 1740 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETUPKF: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| mov edi, ((int)in->a)
        //| mov64 rsi, in->ival
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov rdx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetupkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2151, ((int)in->a), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetupkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetupkf)>>32), ((int)in->pc));
#line 1754 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETKDB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkdb
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2186, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkdb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkdb)>>32), ((int)in->pc));
#line 1767 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov edx, ((int)in->nval)
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkb
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2222, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)in->nval), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkb), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkb)>>32), ((int)in->pc));
#line 1779 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKT:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkt
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2186, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkt), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkt)>>32), ((int)in->pc));
#line 1791 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKS:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetks
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2186, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetks)>>32), ((int)in->pc));
#line 1803 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKKS: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov64 rdx, (int64_t)bits
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkks
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2256, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkks), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkks)>>32), ((int)in->pc));
#line 1817 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETKKF: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov64 rdx, (int64_t)bits
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2256, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)bits), (unsigned int)(((int64_t)bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkkf)>>32), ((int)in->pc));
#line 1832 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETKF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkf
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2186, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkf), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkf)>>32), ((int)in->pc));
#line 1845 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETKK:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| mov64 rdx, (int64_t)in->nval
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetkk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2256, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), (unsigned int)((int64_t)in->nval), (unsigned int)(((int64_t)in->nval)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetkk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetkk)>>32), ((int)in->pc));
#line 1857 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETK:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tsetk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2186, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tsetk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tsetk)>>32), ((int)in->pc));
#line 1869 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgeti
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2292, ((int)LUAR_SLOT(in->b)), in->ival, ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgeti), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgeti)>>32), ((int)in->pc));
#line 1879 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSETIF:
      case LUAR_IR_TSETIS:
      case LUAR_IR_TSETIB:
      case LUAR_IR_TSETIT: {
        const void *fn = (in->kind == LUAR_IR_TSETIF) ?
          (const void *)&luar_trace_tsetif :
          (in->kind == LUAR_IR_TSETIS) ?
          (const void *)&luar_trace_tsetis :
          (in->kind == LUAR_IR_TSETIB) ?
          (const void *)&luar_trace_tsetib :
          (const void *)&luar_trace_tsetit;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)fn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2186, ((int)LUAR_SLOT(in->a)), (unsigned int)(in->ival), (unsigned int)((in->ival)>>32), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)fn), (unsigned int)(((int64_t)(intptr_t)fn)>>32), ((int)in->pc));
#line 1901 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_TSETI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, in->ival
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tseti
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2321, ((int)LUAR_SLOT(in->a)), in->ival, ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tseti), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tseti)>>32), ((int)in->pc));
#line 1914 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2II:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov esi, dword [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov edx, dword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2ii
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2357, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2ii), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2ii)>>32), ((int)in->pc));
#line 1925 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2ID:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2id
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2388, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2id), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2id)>>32), ((int)in->pc));
#line 1936 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2DI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movq xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2di
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2423, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2di), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2di)>>32), ((int)in->pc));
#line 1947 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2FI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2fi
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2458, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2fi), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2fi)>>32), ((int)in->pc));
#line 1958 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2FF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movq xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movq xmm1, qword [rbx + ((int)LUAR_SLOT((int)in->ival))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2ff
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2493, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT((int)in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2ff), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2ff)>>32), ((int)in->pc));
#line 1969 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL2DD:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(in->ival))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall2dd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2530, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->ival)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall2dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall2dd)>>32), ((int)in->pc));
#line 1980 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CCALL3DD: {
        int a2 = (int)(in->ival & 0xff);
        int a3 = (int)((in->ival >> 8) & 0xff);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| movsd xmm1, qword [rbx + ((int)LUAR_SLOT(a2))]
        //| movsd xmm2, qword [rbx + ((int)LUAR_SLOT(a3))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall3dd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2567, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(a2)), ((int)LUAR_SLOT(a3)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall3dd), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall3dd)>>32), ((int)in->pc));
#line 1994 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CCALL1I:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_ccall1i
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 2610, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_ccall1i), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_ccall1i)>>32), ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2006 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CPSETI64: {
        const void *setfn = (in->kind == LUAR_IR_CPSETF) ?
          (const void *)&luar_ffi_trace_cset_ptr_double :
          (in->kind == LUAR_IR_CPSETI64 ?
           (const void *)&luar_ffi_trace_cset_ptr_i64 :
           (const void *)&luar_ffi_trace_cset_ptr_int);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, ((int64_t)in->ival)
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)setfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2643, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)setfn), (unsigned int)(((int64_t)(intptr_t)setfn)>>32), ((int)in->pc));
#line 2022 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CPGETI:
      case LUAR_IR_CPGETF:
      case LUAR_IR_CPGETI64: {
        const void *getfn = (in->kind == LUAR_IR_CPGETF) ?
          (const void *)&luar_ffi_trace_cget_ptr_double :
          (in->kind == LUAR_IR_CPGETI64 ?
           (const void *)&luar_ffi_trace_cget_ptr_i64 :
           (const void *)&luar_ffi_trace_cget_ptr_int);
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, ((int64_t)in->ival)
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)getfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2643, ((int)LUAR_SLOT(in->b)), ((int64_t)in->ival), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)getfn), (unsigned int)(((int64_t)(intptr_t)getfn)>>32), ((int)in->pc));
#line 2041 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CSETFIELDK: {
        int isf = (in->c == 2);
        const void *cfn = isf ?
          (const void *)&luar_ffi_trace_cset_field_double_const :
          (const void *)&luar_ffi_trace_cset_field_int_const;
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, ((int64_t)in->ival)
        //| mov rdx, ((int64_t)in->b)
        dasm_put(Dst, 2676, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int64_t)in->b));
#line 2053 "src/luar_emit_x64.dasc"
        if (isf) {
          //| mov64 rax, bits
          //| movq xmm0, rax
          dasm_put(Dst, 89, (unsigned int)(bits), (unsigned int)((bits)>>32));
#line 2056 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov rcx, ((int64_t)in->nval)
          dasm_put(Dst, 2689, ((int64_t)in->nval));
#line 2059 "src/luar_emit_x64.dasc"
        }
        //| mov64 rax, (int64_t)(intptr_t)cfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1789, (unsigned int)((int64_t)(intptr_t)cfn), (unsigned int)(((int64_t)(intptr_t)cfn)>>32), ((int)in->pc));
#line 2065 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CSETFIELD: {
        const void *setfn = (in->c == 2) ?
          (const void *)&luar_ffi_trace_cset_field_double :
          (const void *)&luar_ffi_trace_cset_field_int;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, ((int64_t)in->ival)
        //| mov rdx, ((int64_t)in->nval)
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, (int64_t)(intptr_t)setfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2694, ((int)LUAR_SLOT(in->a)), ((int64_t)in->ival), ((int64_t)in->nval), ((int)LUAR_SLOT(in->b)), (unsigned int)((int64_t)(intptr_t)setfn), (unsigned int)(((int64_t)(intptr_t)setfn)>>32), ((int)in->pc));
#line 2080 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CGETFIELD: {
        const void *getfn = (in->c == 2) ?
          (const void *)&luar_ffi_trace_cget_field_double :
          (const void *)&luar_ffi_trace_cget_field_int;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, ((int64_t)in->ival)
        //| mov rdx, ((int64_t)in->nval)
        //| lea rcx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)getfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2694, ((int)LUAR_SLOT(in->b)), ((int64_t)in->ival), ((int64_t)in->nval), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)getfn), (unsigned int)(((int64_t)(intptr_t)getfn)>>32), ((int)in->pc));
#line 2095 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CSETF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_cset_double
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1864, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cset_double), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cset_double)>>32), ((int)in->pc));
#line 2106 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CSETI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_cset_int
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1864, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cset_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cset_int)>>32), ((int)in->pc));
#line 2116 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CGETI:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_ffi_trace_cget_int
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1864, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_ffi_trace_cget_int), (unsigned int)(((int64_t)(intptr_t)&luar_ffi_trace_cget_int)>>32), ((int)in->pc));
#line 2126 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGET:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        dasm_put(Dst, 2727, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)));
#line 2131 "src/luar_emit_x64.dasc"
        if (side) {
          //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tget_generic
          dasm_put(Dst, 2740, (unsigned int)((int64_t)(intptr_t)&luar_trace_tget_generic), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tget_generic)>>32));
#line 2133 "src/luar_emit_x64.dasc"
        }
        else {
          //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tget_int
          dasm_put(Dst, 2740, (unsigned int)((int64_t)(intptr_t)&luar_trace_tget_int), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tget_int)>>32));
#line 2136 "src/luar_emit_x64.dasc"
        }
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 2626, ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2142 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TSET: {
        const void *tsetfn = (const void *)&luar_trace_tset_insert;
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov rcx, r12
        //| mov64 rax, (int64_t)(intptr_t)tsetfn
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test rax, rax
        //| jz >2
        //| mov rbx, rax
        dasm_put(Dst, 2745, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)tsetfn), (unsigned int)(((int64_t)(intptr_t)tsetfn)>>32), ((int)in->pc));
#line 2155 "src/luar_emit_x64.dasc"
        /* GC may have relocated the stack. */
        break;
      }
      case LUAR_IR_ADDK:
      case LUAR_IR_SUBK:
      case LUAR_IR_MULK:
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->b))]
        dasm_put(Dst, 185, ((int)LUAR_SLOT(in->b)));
#line 2162 "src/luar_emit_x64.dasc"
        if (in->kind == LUAR_IR_ADDK) {
          //| add rax, in->ival
          dasm_put(Dst, 190, in->ival);
#line 2164 "src/luar_emit_x64.dasc"
        }
        else if (in->kind == LUAR_IR_SUBK) {
          //| sub rax, in->ival
          dasm_put(Dst, 195, in->ival);
#line 2167 "src/luar_emit_x64.dasc"
        }
        else {
          //| imul rax, in->ival
          dasm_put(Dst, 200, in->ival);
#line 2170 "src/luar_emit_x64.dasc"
        }
        //| mov qword [rbx + ((int)LUAR_SLOT(in->a))], rax
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMINT
        dasm_put(Dst, 57, ((int)LUAR_SLOT(in->a)), ((int)LUAR_TAG(in->a)), LUA_VNUMINT);
#line 2173 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_CGUARD:
        //| mov r11d, ((int)in->pc)
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rcx, (int64_t)(in->ival)
        //| cmp rax, rcx
        //| jne >8
        dasm_put(Dst, 2781, ((int)in->pc), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(in->ival)), (unsigned int)(((int64_t)(in->ival))>>32));
#line 2180 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_TGETF:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov rsi, qword [rbx + ((int)LUAR_SLOT(in->c))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->a))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_tgetf_generic
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        //| mov byte [rbx + ((int)LUAR_TAG(in->a))], LUA_VNUMFLT
        dasm_put(Dst, 2610, ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), ((int)LUAR_SLOT(in->a)), (unsigned int)((int64_t)(intptr_t)&luar_trace_tgetf_generic), (unsigned int)(((int64_t)(intptr_t)&luar_trace_tgetf_generic)>>32), ((int)in->pc), ((int)LUAR_TAG(in->a)), LUA_VNUMFLT);
#line 2191 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_FADDI: {
        uint64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_faddk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2800, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_faddk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_faddk)>>32), ((int)in->pc));
#line 2204 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_FMULK: {
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_fmulk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2800, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_fmulk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fmulk)>>32), ((int)in->pc));
#line 2218 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_FADD:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_fadd
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1864, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_fadd), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fadd)>>32), ((int)in->pc));
#line 2229 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_FSUB:
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| lea rdx, [rbx + ((int)LUAR_SLOT(in->c))]
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_fsub
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 1864, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), ((int)LUAR_SLOT(in->c)), (unsigned int)((int64_t)(intptr_t)&luar_trace_fsub), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fsub)>>32), ((int)in->pc));
#line 2239 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_FDIVK: {
        int64_t bits;
        memcpy(&bits, &in->nval, sizeof(bits));
        //| lea rdi, [rbx + ((int)LUAR_SLOT(in->a))]
        //| lea rsi, [rbx + ((int)LUAR_SLOT(in->b))]
        //| mov64 rax, bits
        //| movq xmm0, rax
        //| mov64 rax, (int64_t)(intptr_t)&luar_trace_fdivk
        //| call rax
        //| mov r11d, ((int)in->pc)
        //| test eax, eax
        //| jz >2
        dasm_put(Dst, 2800, ((int)LUAR_SLOT(in->a)), ((int)LUAR_SLOT(in->b)), (unsigned int)(bits), (unsigned int)((bits)>>32), (unsigned int)((int64_t)(intptr_t)&luar_trace_fdivk), (unsigned int)(((int64_t)(intptr_t)&luar_trace_fdivk)>>32), ((int)in->pc));
#line 2252 "src/luar_emit_x64.dasc"
        break;
      }
      case LUAR_IR_CALLEXIT:
        //| mov r11d, ((int)in->pc)
        //| jmp >8
        dasm_put(Dst, 2834, ((int)in->pc));
#line 2257 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_FORLOOP:
        if (forslot >= 0)
          return 0;
        forslot = in->a;
        if (side) {
          //| mov rax, qword [rbx + ((int)LUAR_SLOT(forslot))]
          //| test rax, rax
          //| jz >5
          //| dec rax
          //| mov qword [rbx + ((int)LUAR_SLOT(forslot))], rax
          //| mov rcx, qword [rbx + ((int)LUAR_SLOT(forslot + 2))]
          //| add rcx, qword [rbx + ((int)LUAR_SLOT(forslot + 1))]
          //| mov qword [rbx + ((int)LUAR_SLOT(forslot + 2))], rcx
          //| xor eax, eax
          //| add rsp, 16
          //| pop rbx
          //| pop r15
          //| pop r12
          //| ret
          //|5:
          //| mov eax, 1
          //| add rsp, 16
          //| pop rbx
          //| pop r15
          //| pop r12
          //| ret
          dasm_put(Dst, 2842, ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot + 2)), ((int)LUAR_SLOT(forslot + 1)), ((int)LUAR_SLOT(forslot + 2)));
#line 2284 "src/luar_emit_x64.dasc"
          break;
        }
        //| mov rax, qword [rbx + ((int)LUAR_SLOT(forslot))]
        //| test rax, rax
        //| jz >3
        //| dec rax
        //| mov qword [rbx + ((int)LUAR_SLOT(forslot))], rax
        //| mov rcx, qword [rbx + ((int)LUAR_SLOT(forslot + 2))]
        //| add rcx, qword [rbx + ((int)LUAR_SLOT(forslot + 1))]
        //| mov qword [rbx + ((int)LUAR_SLOT(forslot + 2))], rcx
        //| dec r15d
        //| jz >6
        //| jmp <1
        dasm_put(Dst, 2903, ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot)), ((int)LUAR_SLOT(forslot + 2)), ((int)LUAR_SLOT(forslot + 1)), ((int)LUAR_SLOT(forslot + 2)));
#line 2297 "src/luar_emit_x64.dasc"
        break;
      case LUAR_IR_EXIT:
      case LUAR_IR_RET:
        break;
      default:
        return 0;
    }
  }
  //|3:
  //| mov eax, 1
  //| add rsp, 16
  //| pop rbx
  //| pop r15
  //| pop r12
  //| ret
  //|2:
  dasm_put(Dst, 2947);
#line 2313 "src/luar_emit_x64.dasc"
  if (side) {
    //| mov eax, r11d
    //| neg eax
    //| sub eax, 2
    dasm_put(Dst, 2967);
#line 2317 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov eax, r11d
    //| neg eax
    //| sub eax, 2
    dasm_put(Dst, 2967);
#line 2322 "src/luar_emit_x64.dasc"
  }
  //| add rsp, 16
  //| pop rbx
  //| pop r15
  //| pop r12
  //| ret
  //|6:
  //| mov eax, 2
  //| add rsp, 16
  //| pop rbx
  //| pop r15
  //| pop r12
  //| ret
  //|8:
  //| mov eax, r11d
  //| neg eax
  //| sub eax, 2
  //| add rsp, 16
  //| pop rbx
  //| pop r15
  //| pop r12
  //| ret
  dasm_put(Dst, 2977);
#line 2344 "src/luar_emit_x64.dasc"
  return 1;
}

int luari_emit_trace_ir (const LuaR_TraceIR *t, void **code, size_t *size) {
  dasm_State *Dst;
  size_t sz = 0;
  void *mem;
  int status;
  if (t == NULL || code == NULL || size == NULL || t->nins <= 0)
    return 0;
  dasm_init(&Dst, DASM_MAXSECTION);
  dasm_setupglobal(&Dst, NULL, 0);
  dasm_setup(&Dst, luar_emit_actions);
  if (!emit_trace_ir_func(&Dst, t)) {
    dasm_free(&Dst);
    return 0;
  }
  if (dasm_checkstep(&Dst, -1) != 0 ||
      dasm_link(&Dst, &sz) != 0) {
    dasm_free(&Dst);
    return 0;
  }
  mem = mmap(NULL, sz, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED) {
    dasm_free(&Dst);
    return 0;
  }
  status = dasm_encode(&Dst, mem);
  if (status != 0 || mprotect(mem, sz, PROT_READ | PROT_EXEC) != 0) {
    if (status == 0) munmap(mem, sz);
    dasm_free(&Dst);
    return 0;
  }
  dasm_free(&Dst);
  *code = mem;
  *size = sz;
  return 1;
}


LuaR_LoopFn luari_emit_build (const LuaR_LoopDesc *d, size_t *code_size)
{
  dasm_State *Dst;
  size_t size = 0;
  void *mem;
  int status;
  if (d == NULL || code_size == NULL || d->ops == NULL || d->nops <= 0 ||
      (!d->is_while && d->for_a < 0) || d->for_a + 2 > 254)
    return NULL;
  dasm_init(&Dst, DASM_MAXSECTION);
  dasm_setupglobal(&Dst, NULL, 0);
  dasm_setup(&Dst, luar_emit_actions);
  if (d->has_branch && d->npc > 0)
    dasm_growpc(&Dst, (unsigned)d->npc + 2);
  emit_code(&Dst, d);
  if ((status = dasm_checkstep(&Dst, -1)) != 0) {
    dasm_free(&Dst);
    return NULL;
  }
  if ((status = dasm_link(&Dst, &size)) != 0) {
    dasm_free(&Dst);
    return NULL;
  }
  mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED) {
    dasm_free(&Dst);
    return NULL;
  }
  if ((status = dasm_encode(&Dst, mem)) != 0) {
    munmap(mem, size);
    dasm_free(&Dst);
    return NULL;
  }
  if (mprotect(mem, size, PROT_READ | PROT_EXEC) != 0) {
    munmap(mem, size);
    dasm_free(&Dst);
    return NULL;
  }
  dasm_free(&Dst);
  *code_size = size;
  return (LuaR_LoopFn)mem;
}
