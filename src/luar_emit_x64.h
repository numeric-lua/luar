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
static const unsigned char luar_emit_actions[1468] = {
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
  76,139,163,233,73,139,132,253,36,233,72,133,192,15,133,244,249,77,139,172,
  253,36,233,69,139,180,253,36,233,255,252,233,245,255,252,233,245,249,184,
  1,0,0,0,255,65,94,65,93,65,92,255,91,195,255,249,72,139,131,233,72,133,192,
  15,132,244,248,72,252,255,200,72,137,131,233,72,139,139,233,72,3,139,233,
  72,137,139,233,252,233,245,249,248,2,184,1,0,0,0,255,248,3,49,192,255,83,
  72,137,252,251,65,84,65,85,65,86,255,76,139,163,233,76,139,171,233,76,139,
  179,233,252,242,15,16,131,233,72,184,237,237,102,72,15,110,200,248,1,252,
  242,73,15,42,213,252,242,15,89,209,252,242,15,88,194,77,133,228,15,132,244,
  248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,163,233,76,137,
  171,233,252,242,15,17,131,233,184,1,0,0,0,65,94,65,93,65,92,91,195,248,3,
  49,192,65,94,65,93,65,92,91,195,255,83,72,137,252,251,65,84,65,85,65,86,65,
  87,76,139,163,233,76,139,171,233,76,139,179,233,255,76,139,187,233,73,139,
  135,233,72,133,192,15,133,244,249,77,139,151,233,69,139,159,233,248,1,73,
  131,252,253,1,15,140,244,249,77,57,221,15,135,244,249,76,137,232,72,252,255,
  200,72,193,224,4,76,1,208,76,137,40,198,128,233,235,77,133,228,15,132,244,
  248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,163,233,255,76,
  137,171,233,184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,76,137,163,233,
  76,137,171,233,49,192,65,95,65,94,65,93,65,92,91,195,255,83,72,137,252,251,
  65,84,65,85,65,86,65,87,76,139,163,233,76,139,171,233,76,139,179,233,76,139,
  187,233,255,248,1,77,1,252,239,77,133,228,15,132,244,248,73,252,255,204,77,
  1,252,245,252,233,244,1,248,2,76,137,163,233,76,137,171,233,76,137,187,233,
  184,1,0,0,0,65,95,65,94,65,93,65,92,91,195,248,3,76,137,163,233,76,137,171,
  233,49,192,65,95,65,94,65,93,65,92,91,195,255,76,139,131,233,73,139,128,233,
  72,133,192,15,133,244,249,77,139,136,233,69,139,144,233,255,73,131,252,254,
  1,15,133,244,249,73,131,252,253,1,15,140,244,249,76,137,232,76,1,224,76,57,
  208,15,135,244,249,76,137,252,233,72,252,255,201,72,193,225,4,76,1,201,76,
  137,226,72,252,255,194,248,4,128,185,233,235,15,133,244,249,72,131,193,16,
  72,252,255,202,15,133,244,4,77,137,252,235,73,252,255,203,73,193,227,4,77,
  1,203,248,1,255,77,3,59,255,252,242,73,15,42,11,252,242,15,88,193,255,73,
  131,195,16,77,133,228,15,132,244,248,73,252,255,204,73,129,197,239,252,233,
  244,1,248,2,76,137,163,233,76,137,171,233,255,76,137,187,233,255,184,1,0,
  0,0,65,95,65,94,65,93,65,92,91,195,248,3,76,137,163,233,76,137,171,233,255,
  83,72,137,252,251,65,84,65,85,65,86,76,139,163,233,76,139,171,233,255,73,
  190,237,237,255,248,1,77,57,252,245,255,15,140,244,248,255,15,141,244,248,
  255,15,142,244,248,255,15,143,244,248,255,76,137,252,233,73,15,175,205,73,
  1,204,255,77,1,252,236,255,73,129,197,239,252,233,244,1,248,2,76,137,163,
  233,76,137,171,233,184,1,0,0,0,65,94,65,93,65,92,91,195,248,3,49,192,65,94,
  65,93,65,92,91,195,255,248,1,76,137,252,233,73,15,175,205,73,1,207,77,133,
  228,15,132,244,248,73,252,255,204,77,1,252,245,252,233,244,1,248,2,76,137,
  163,233,76,137,171,233,76,137,187,233,184,1,0,0,0,65,95,65,94,65,93,65,92,
  91,195,248,3,49,192,65,95,65,94,65,93,65,92,91,195,255,72,139,131,233,72,
  133,192,15,132,244,248,72,252,255,200,72,137,131,233,72,139,139,233,72,3,
  139,233,72,137,139,233,252,233,244,1,248,2,184,1,0,0,0,91,195,248,3,49,192,
  91,195,255
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
  emit_guards(Dst, d);
  if (d->has_table) {
    //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->table_reg))]
    //| mov rax, qword [r12 + ((int)d->table_metatable_off)]
    //| test rax, rax
    //| jnz >3
    //| mov r13, qword [r12 + ((int)d->table_array_off)]
    //| mov r14d, dword [r12 + ((int)d->table_asize_off)]
    dasm_put(Dst, 546, ((int)LUAR_SLOT(d->table_reg)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 496 "src/luar_emit_x64.dasc"
  }
  for (i = 0; i < d->nops; i++) {
    const LuaR_Op *op = &d->ops[i];
    //|=>((int)i):
    dasm_put(Dst, 7, ((int)i));
#line 500 "src/luar_emit_x64.dasc"
    switch (op->kind) {
      case LUAR_OP_LT:
      case LUAR_OP_LE:
      case LUAR_OP_GT:
      case LUAR_OP_GE:
      case LUAR_OP_EQ:
        emit_cmp(Dst, d, op, i + 2);
        /* false/true fall-through skips the paired JMP */
        //| jmp =>((int)(i + 2))
        dasm_put(Dst, 576, ((int)(i + 2)));
#line 509 "src/luar_emit_x64.dasc"
        break;
      case LUAR_OP_JMP:
        //| jmp =>((int)op->target)
        dasm_put(Dst, 576, ((int)op->target));
#line 512 "src/luar_emit_x64.dasc"
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
    //| jmp =>0
    //|=>((int)d->nops):
    //| mov eax, 1
    dasm_put(Dst, 580, 0, ((int)d->nops));
#line 534 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 590);
#line 538 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 597);
#line 541 "src/luar_emit_x64.dasc"
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
    dasm_put(Dst, 600, ((int)d->nops), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)), 0, ((int)(d->nops + 1)));
#line 556 "src/luar_emit_x64.dasc"
    if (d->has_table) {
      //| pop r14
      //| pop r13
      //| pop r12
      dasm_put(Dst, 590);
#line 560 "src/luar_emit_x64.dasc"
    }
    //| pop rbx
    //| ret
    dasm_put(Dst, 597);
#line 563 "src/luar_emit_x64.dasc"
  }
  //|3:
  //| xor eax, eax
  dasm_put(Dst, 644);
#line 566 "src/luar_emit_x64.dasc"
  if (d->has_table) {
    //| pop r14
    //| pop r13
    //| pop r12
    dasm_put(Dst, 590);
#line 570 "src/luar_emit_x64.dasc"
  }
  //| pop rbx
  //| ret
  dasm_put(Dst, 597);
#line 573 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 649);
#line 584 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->for_a))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->ff_idx))]
  //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->for_a + 1))]
  //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ff_acc))]
  //| mov64 rax, bits
  //| movq xmm1, rax
  //|1:
  //| cvtsi2sd xmm2, r13
  //| mulsd xmm2, xmm1
  //| addsd xmm0, xmm2
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| movsd qword [rbx + ((int)LUAR_SLOT(d->ff_acc))], xmm0
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
  dasm_put(Dst, 661, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ff_idx)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->ff_acc)), (unsigned int)(bits), (unsigned int)((bits)>>32), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->ff_acc)));
#line 617 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 767, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 630 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 793, ((int)LUAR_SLOT(d->ff_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off), 8, ((int)d->tag_int), ((int)LUAR_SLOT(d->for_a)));
#line 656 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 881, ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 673 "src/luar_emit_x64.dasc"
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
  //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fi_acc))]
  dasm_put(Dst, 923, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fi_acc)));
#line 687 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
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
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fi_acc))], r15
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
  dasm_put(Dst, 953, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fi_acc)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 716 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 767, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->ft_idx)), ((int)LUAR_SLOT(d->for_a + 1)));
#line 730 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 948, ((int)LUAR_SLOT(d->ft_acc)));
#line 732 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd xmm0, qword [rbx + ((int)LUAR_SLOT(d->ft_acc))]
    dasm_put(Dst, 20, ((int)LUAR_SLOT(d->ft_acc)));
#line 735 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //| mov r8, qword [rbx + ((int)LUAR_SLOT(d->ft_table))]
  //| mov rax, qword [r8 + ((int)d->table_metatable_off)]
  //| test rax, rax
  //| jnz >3
  //| mov r9, qword [r8 + ((int)d->table_array_off)]
  //| mov r10d, dword [r8 + ((int)d->table_asize_off)]
  dasm_put(Dst, 1030, ((int)LUAR_SLOT(d->ft_table)), ((int)d->table_metatable_off), ((int)d->table_array_off), ((int)d->table_asize_off));
#line 743 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1054, 8, ((int)d->tag_int));
#line 769 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| add r15, qword [r11]
    dasm_put(Dst, 1147);
#line 771 "src/luar_emit_x64.dasc"
  }
  else {
    //| cvtsi2sd xmm1, qword [r11]
    //| addsd xmm0, xmm1
    dasm_put(Dst, 1151);
#line 775 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1163, d->fw_step, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 785 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 1197, ((int)LUAR_SLOT(d->ft_acc)));
#line 787 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 790 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1202, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 801 "src/luar_emit_x64.dasc"
  if (!isf) {
    //| mov qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], r15
    dasm_put(Dst, 1197, ((int)LUAR_SLOT(d->ft_acc)));
#line 803 "src/luar_emit_x64.dasc"
  }
  else {
    //| movsd qword [rbx + ((int)LUAR_SLOT(d->ft_acc))], xmm0
    dasm_put(Dst, 105, ((int)LUAR_SLOT(d->ft_acc)));
#line 806 "src/luar_emit_x64.dasc"
  }
  //| xor eax, eax
  //| pop r15
  //| pop r14
  //| pop r13
  //| pop r12
  //| pop rbx
  //| ret
  dasm_put(Dst, 910);
#line 814 "src/luar_emit_x64.dasc"
}

static void emit_fast_while (dasm_State **Dst, const LuaR_LoopDesc *d)
{
  int is_lt = (d->fw_kind == LUAR_OP_LT);
  //| push rbx
  //| mov rbx, rdi
  //| push r12
  //| push r13
  //| push r14
  //| mov r12, qword [rbx + ((int)LUAR_SLOT(d->fw_acc))]
  //| mov r13, qword [rbx + ((int)LUAR_SLOT(d->fw_idx))]
  dasm_put(Dst, 1228, ((int)LUAR_SLOT(d->fw_acc)), ((int)LUAR_SLOT(d->fw_idx)));
#line 826 "src/luar_emit_x64.dasc"
  if (d->fw_limit == 0xFF) {
    //| mov64 r14, d->fw_imm
    dasm_put(Dst, 1248, (unsigned int)(d->fw_imm), (unsigned int)((d->fw_imm)>>32));
#line 828 "src/luar_emit_x64.dasc"
  }
  else {
    //| mov r14, qword [rbx + ((int)LUAR_SLOT(d->fw_limit))]
    dasm_put(Dst, 788, ((int)LUAR_SLOT(d->fw_limit)));
#line 831 "src/luar_emit_x64.dasc"
  }
  emit_guards(Dst, d);
  //|1:
  //| cmp r13, r14
  dasm_put(Dst, 1253);
#line 835 "src/luar_emit_x64.dasc"
  if (is_lt) {
    if (d->fw_k) {
      //| jl >2
      dasm_put(Dst, 1260);
#line 838 "src/luar_emit_x64.dasc"
    }
    else {
      //| jge >2
      dasm_put(Dst, 1265);
#line 841 "src/luar_emit_x64.dasc"
    }
  }
  else {
    if (d->fw_k) {
      //| jle >2
      dasm_put(Dst, 1270);
#line 846 "src/luar_emit_x64.dasc"
    }
    else {
      //| jg >2
      dasm_put(Dst, 1275);
#line 849 "src/luar_emit_x64.dasc"
    }
  }
  if (d->fw_square) {
    //| mov rcx, r13
    //| imul rcx, r13
    //| add r12, rcx
    dasm_put(Dst, 1280);
#line 855 "src/luar_emit_x64.dasc"
  }
  else {
    //| add r12, r13
    dasm_put(Dst, 1292);
#line 858 "src/luar_emit_x64.dasc"
  }
  //| add r13, d->fw_step
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fw_acc))], r12
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
  dasm_put(Dst, 1297, d->fw_step, ((int)LUAR_SLOT(d->fw_acc)), ((int)LUAR_SLOT(d->fw_idx)));
#line 877 "src/luar_emit_x64.dasc"
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
  //| mov r15, qword [rbx + ((int)LUAR_SLOT(d->fs_acc))]
  dasm_put(Dst, 923, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->fs_acc)));
#line 891 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //|1:
  //| mov rcx, r13
  //| imul rcx, r13
  //| add r15, rcx
  //| test r12, r12
  //| jz >2
  //| dec r12
  //| add r13, r14
  //| jmp <1
  //|2:
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a))], r12
  //| mov qword [rbx + ((int)LUAR_SLOT(d->for_a + 2))], r13
  //| mov qword [rbx + ((int)LUAR_SLOT(d->fs_acc))], r15
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
  dasm_put(Dst, 1341, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->fs_acc)));
#line 920 "src/luar_emit_x64.dasc"
}

static void emit_code (dasm_State **Dst, const LuaR_LoopDesc *d)
{
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
#line 954 "src/luar_emit_x64.dasc"
  emit_guards(Dst, d);
  //|1:
  dasm_put(Dst, 1144);
#line 956 "src/luar_emit_x64.dasc"
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
  dasm_put(Dst, 1417, ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a)), ((int)LUAR_SLOT(d->for_a + 2)), ((int)LUAR_SLOT(d->for_a + 1)), ((int)LUAR_SLOT(d->for_a + 2)));
#line 977 "src/luar_emit_x64.dasc"
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
